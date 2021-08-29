#include "mir_compiler.hpp"
#include "exception.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <stack>

extern "C"
{
#include "mir-gen.h"
#include "mir.h"
}

using namespace bfjit;

namespace
{
constexpr inline auto BeginArgName = "begin";
constexpr inline auto EndArgName = "end";
constexpr inline auto CurrentPtrArgName = "current";
constexpr inline auto MainFuncName = "main";
constexpr inline auto ReadCharFuncName = "readChar";
constexpr inline auto WriteCharFuncName = "writeChar";
constexpr inline auto ModuleName = "bfjit";

struct LabelPair
{
    MIR_label_t OpenLabel;
    MIR_label_t CloseLabel;
};

struct Argument
{
    const char *Name;
    MIR_type_t Type;

    explicit constexpr Argument(const char *name, MIR_type_t type) noexcept : Name(name), Type(type)
    {
    }
};

template <typename... Types> auto MakeResultTypes(const Types &...types)
{
    return std::array<MIR_type_t, sizeof...(Types)>{types...};
}

template <typename... Types> auto MakeArguments(const Types &...types)
{
    auto GetType = [](const auto &item) {
        return MIR_var_t{
            .type = item.Type,
            .name = item.Name,
            .size = 0u,
        };
    };
    return std::array<MIR_var_t, sizeof...(Types)>{GetType(types)...};
}

struct CompilationUnit
{
    MIR_context_t Mir;
    WriteCharFunc WriteChar;
    ReadCharFunc ReadChar;
    InstructionReader &Reader;
    std::stack<LabelPair> Labels;
    MIR_item_t FuncItem = nullptr;
    MIR_reg_t BeginArgReg = 0;
    MIR_reg_t EndArgReg = 0;
    MIR_reg_t CurrentPtrReg = 0;
    MIR_label_t OutOfMemoryErrorLabel = nullptr;
    MIR_label_t MemoryUnderrunErrorLabel = nullptr;
    std::uint32_t TempRegCounter = 0;
    MIR_item_t WriteCharFuncProto = nullptr;
    MIR_item_t ReadCharFuncProto = nullptr;
    MIR_module_t Module = nullptr;

    MainFunc Compile()
    {
        assert(Mir);
        assert(WriteChar);
        assert(ReadChar);

        BeginModule();
        BeginFunction();
        EmitInstructions();
        EndFunction();
        EndModule();

        return Link();
    }

    void BeginModule()
    {
        assert(!Module);

        Module = MIR_new_module(Mir, ModuleName);
    }

    void EndModule()
    {
        assert(Module);

        MIR_finish_module(Mir);
    }

    MainFunc Link()
    {
        assert(Module);
        assert(FuncItem);

        MIR_load_module(Mir, Module);
        MIR_link(Mir, MIR_set_gen_interface, nullptr);
        const auto entrypoint = MIR_gen(Mir, 0, FuncItem);

        return reinterpret_cast<MainFunc>(entrypoint);
    }

    void BeginFunction()
    {
        assert(!BeginArgReg);
        assert(!EndArgReg);
        assert(!CurrentPtrReg);
        assert(!FuncItem);
        assert(!ReadCharFuncProto);
        assert(!WriteCharFuncProto);
        assert(!MemoryUnderrunErrorLabel);
        assert(!OutOfMemoryErrorLabel);

        ReadCharFuncProto =
            NewFunctionPrototype(ReadCharFuncName, MakeResultTypes(MIR_T_I64), MakeArguments(Argument("ptr", MIR_T_P)));
        WriteCharFuncProto = NewFunctionPrototype(WriteCharFuncName, MakeResultTypes(MIR_T_I64),
                                                  MakeArguments(Argument("value", MIR_T_I64)));

        FuncItem = NewFunction(MainFuncName, MakeResultTypes(MIR_T_I64),
                               MakeArguments(Argument(BeginArgName, MIR_T_I64), Argument(EndArgName, MIR_T_I64)));

        BeginArgReg = GetReg(BeginArgName);
        EndArgReg = GetReg(EndArgName);

        CurrentPtrReg = NewReg(CurrentPtrArgName);
        AddInstruction(MIR_MOV, NewRegOp(CurrentPtrReg), NewRegOp(BeginArgReg));

        MemoryUnderrunErrorLabel = NewLabel();
        OutOfMemoryErrorLabel = NewLabel();
    }

    void EndFunction()
    {
        assert(Labels.empty());
        assert(OutOfMemoryErrorLabel);
        assert(MemoryUnderrunErrorLabel);

        AppendRetInstruction(Result::Success);

        std::array errorHandlers = {
            std::make_pair(OutOfMemoryErrorLabel, Result::OutOfMemory),
            std::make_pair(MemoryUnderrunErrorLabel, Result::MemoryUnderrun),
        };
        for (const auto [label, result] : errorHandlers)
        {
            AppendInstruction(label);
            AppendRetInstruction(result);
        }

        MIR_finish_func(Mir);
    }

    void EmitInstructions()
    {
        assert(Labels.empty());

        while (true)
        {
            const auto instruction = Reader.Next();
            if (instruction == Instruction::Invalid)
            {
                break;
            }
            else
            {
                switch (instruction)
                {
                case Instruction::Inc:
                    EmitIncInstruction();
                    break;
                case Instruction::Dec:
                    EmitDecInstruction();
                    break;
                case Instruction::Next:
                    EmitNextInstruction();
                    break;
                case Instruction::Prev:
                    EmitPrevInstruction();
                    break;
                case Instruction::Jz:
                    EmitJzInstruction();
                    break;
                case Instruction::Jnz:
                    EmitJnzInstruction();
                    break;
                case Instruction::WriteChar:
                    EmitWriteCharInstruction();
                    break;
                case Instruction::ReadChar:
                    EmitReadCharInstruction();
                    break;
                case Instruction::Invalid:
                    assert(false);
                    break;
                }
            }
        }

        if (!Labels.empty())
        {
            throw Exception("no matching close label found");
        }
    }

    void EmitIncInstruction()
    {
        const auto currentValue = LoadCurrent();
        AddInstruction(MIR_ADD, NewRegOp(currentValue), NewRegOp(currentValue), NewIntOp(1));
        StoreCurrent(currentValue);
    }

    void EmitDecInstruction()
    {
        const auto currentValue = LoadCurrent();
        AddInstruction(MIR_SUB, NewRegOp(currentValue), NewRegOp(currentValue), NewIntOp(1));
        StoreCurrent(currentValue);
    }

    void EmitNextInstruction()
    {
        assert(OutOfMemoryErrorLabel);

        AddInstruction(MIR_BEQ, NewLabelOp(OutOfMemoryErrorLabel), NewRegOp(CurrentPtrReg), NewRegOp(EndArgReg));
        AddInstruction(MIR_ADD, NewRegOp(CurrentPtrReg), NewRegOp(CurrentPtrReg), NewIntOp(1));
    }

    void EmitPrevInstruction()
    {
        assert(MemoryUnderrunErrorLabel);

        AddInstruction(MIR_BEQ, NewLabelOp(MemoryUnderrunErrorLabel), NewRegOp(CurrentPtrReg), NewRegOp(BeginArgReg));
        AddInstruction(MIR_SUB, NewRegOp(CurrentPtrReg), NewRegOp(CurrentPtrReg), NewIntOp(1));
    }

    void EmitJzInstruction()
    {
        const auto openLabel = NewLabel();
        const auto closeLabel = NewLabel();
        Labels.push(LabelPair{.OpenLabel = openLabel, .CloseLabel = closeLabel});

        const auto currentValue = LoadCurrent();
        AddInstruction(MIR_BEQ, NewLabelOp(closeLabel), NewRegOp(currentValue), NewIntOp(0));
        AppendInstruction(openLabel);
    }

    void EmitJnzInstruction()
    {
        if (Labels.empty())
        {
            throw Exception("no matching open label found");
        }

        const auto labels = Labels.top();
        Labels.pop();

        assert(labels.OpenLabel);
        assert(labels.CloseLabel);

        const auto currentValue = LoadCurrent();
        AddInstruction(MIR_BNE, NewLabelOp(labels.OpenLabel), NewRegOp(currentValue), NewIntOp(0));
        AppendInstruction(labels.CloseLabel);
    }

    void EmitWriteCharInstruction()
    {
        assert(WriteChar);

        const auto currentValue = LoadCurrent();
        const auto writeStatusValue = NewReg();
        AppendCallInstruction(NewRefOp(WriteCharFuncProto), NewFuncPtrOp(WriteChar), NewRegOp(writeStatusValue),
                              NewRegOp(currentValue));

        const auto writeSuccessLabel = NewLabel();
        AddInstruction(MIR_BEQ, NewLabelOp(writeSuccessLabel), NewRegOp(writeStatusValue), NewIntOp(Result::Success));
        AppendRetInstruction(NewRegOp(writeStatusValue));
        AppendInstruction(writeSuccessLabel);
    }

    void EmitReadCharInstruction()
    {
        assert(ReadChar);

        const auto readStatusValue = NewReg();
        AppendCallInstruction(NewRefOp(ReadCharFuncProto), NewFuncPtrOp(ReadChar), NewRegOp(readStatusValue),
                              NewRegOp(CurrentPtrReg));

        const auto readSuccessLabel = NewLabel();
        AddInstruction(MIR_BEQ, NewLabelOp(readSuccessLabel), NewRegOp(readStatusValue), NewIntOp(Result::Success));
        AppendRetInstruction(NewRegOp(readStatusValue));
        AppendInstruction(readSuccessLabel);
    }

    template <typename RetTypes, typename ArgTypes>
    MIR_item_t NewFunction(const char *name, RetTypes &&retTypes, ArgTypes &&argTypes)
    {
        assert(Mir);
        assert(name);

        return MIR_new_func_arr(Mir, name, retTypes.size(), retTypes.data(), argTypes.size(), argTypes.data());
    }

    template <typename RetTypes, typename ArgTypes>
    MIR_item_t NewFunctionPrototype(const char *name, RetTypes &&retTypes, ArgTypes &&argTypes)
    {
        assert(Mir);
        assert(name);

        return MIR_new_proto_arr(Mir, name, retTypes.size(), retTypes.data(), argTypes.size(), argTypes.data());
    }

    MIR_op_t NewRegOp(MIR_reg_t reg)
    {
        assert(Mir);

        return MIR_new_reg_op(Mir, reg);
    }

    MIR_op_t NewIntOp(std::int64_t value)
    {
        assert(Mir);

        return MIR_new_int_op(Mir, value);
    }

    MIR_op_t NewIntOp(Result result)
    {
        return NewIntOp(static_cast<std::uint32_t>(result));
    }

    MIR_op_t NewLabelOp(MIR_label_t label)
    {
        assert(Mir);
        assert(label);

        return MIR_new_label_op(Mir, label);
    }

    MIR_op_t NewRefOp(MIR_item_t item)
    {
        assert(Mir);
        assert(item);

        return MIR_new_ref_op(Mir, item);
    }

    MIR_label_t NewLabel()
    {
        assert(Mir);

        return MIR_new_label(Mir);
    }

    MIR_reg_t LoadCurrent()
    {
        assert(CurrentPtrReg);

        const auto currentValue = NewReg();
        AddInstruction(MIR_MOV, NewRegOp(currentValue), NewMemOp(CurrentPtrReg));

        return currentValue;
    }

    void StoreCurrent(MIR_reg_t value)
    {
        assert(CurrentPtrReg);

        AddInstruction(MIR_MOV, NewMemOp(CurrentPtrReg), NewRegOp(value));
    }

    MIR_reg_t NewReg(const char *name = nullptr)
    {
        assert(Mir);
        assert(FuncItem);

        char regNameBuffer[64];
        if (!name)
        {
            std::snprintf(regNameBuffer, std::size(regNameBuffer), "temp_%d", TempRegCounter++);
            name = regNameBuffer;
        }

        return MIR_new_func_reg(Mir, FuncItem->u.func, MIR_T_I64, name);
    }

    MIR_op_t NewMemOp(MIR_reg_t pointerReg)
    {
        assert(Mir);

        return MIR_new_mem_op(Mir, MIR_T_U8, 0, pointerReg, 0, 0);
    }

    template <typename Result, typename... Args> MIR_op_t NewFuncPtrOp(Result (*ptr)(Args...))
    {
        assert(ptr);

        return NewIntOp(reinterpret_cast<std::int64_t>(ptr));
    }

    MIR_reg_t GetReg(const char *name)
    {
        assert(Mir);
        assert(FuncItem);

        return MIR_reg(Mir, name, FuncItem->u.func);
    }

    template <typename... Args> MIR_insn_t NewInstruction(MIR_insn_code_t code, Args &&...args)
    {
        assert(Mir);

        return MIR_new_insn(Mir, code, args...);
    }

    void AppendInstruction(MIR_insn_t instruction)
    {
        assert(Mir);
        assert(FuncItem);

        MIR_append_insn(Mir, FuncItem, instruction);
    }

    void AppendRetInstruction(MIR_op_t result)
    {
        assert(Mir);

        AppendInstruction(MIR_new_ret_insn(Mir, 1, result));
    }

    void AppendRetInstruction(Result result)
    {
        AppendRetInstruction(NewIntOp(result));
    }

    template <typename... Args> void AppendCallInstruction(Args &&...args)
    {
        assert(Mir);

        AppendInstruction(MIR_new_call_insn(Mir, sizeof...(Args), args...));
    }

    template <typename... Args> void AddInstruction(MIR_insn_code_t code, Args &&...args)
    {
        AppendInstruction(NewInstruction(code, args...));
    }
};
} // namespace

struct MirCompiler::Impl
{
    MIR_context_t Mir;

    Impl()
    {
        Mir = MIR_init();
        if (!Mir)
        {
            throw Exception("failed to initialize mir context");
        }

        MIR_gen_init(Mir, 0);
    }

    ~Impl()
    {
        MIR_gen_finish(Mir);
        MIR_finish(Mir);
    }

    MainFunc Compile(const CompilerContext &context)
    {
        assert(Mir);
        assert(context.WriteChar);
        assert(context.ReadChar);
        assert(context.Reader);

        auto compilationUnit = CompilationUnit{
            .Mir = Mir,
            .WriteChar = context.WriteChar,
            .ReadChar = context.ReadChar,
            .Reader = *context.Reader,
        };
        return compilationUnit.Compile();
    }
};

MirCompiler::MirCompiler() : m_impl(std::make_unique<Impl>())
{
}

MirCompiler::~MirCompiler()
{
}

MainFunc MirCompiler::Compile(const CompilerContext &context)
{
    assert(m_impl);

    if (!context.WriteChar)
    {
        throw Exception("write char function must not be null");
    }

    if (!context.ReadChar)
    {
        throw Exception("read char function must not be null");
    }

    if (!context.Reader)
    {
        throw Exception("instruction reader must not be null");
    }

    return m_impl->Compile(context);
}
