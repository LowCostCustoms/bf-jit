#ifndef BFJIT_INSTRUCTION_HPP
#define BFJIT_INSTRUCTION_HPP

namespace bfjit
{
enum class Instruction
{
    Invalid,
    Inc,
    Dec,
    Next,
    Prev,
    Jz,
    Jnz,
    WriteChar,
    ReadChar,
};

struct InstructionReader
{
    virtual ~InstructionReader() = default;
    virtual Instruction Next() = 0;
};

template <typename Iterator> class IteratorInstructionReader final : public InstructionReader
{
public:
    explicit IteratorInstructionReader(Iterator first, Iterator last) : m_first(first), m_last(last)
    {
    }

    Instruction Next() override
    {
        while (m_first != m_last)
        {
            switch (*m_first++)
            {
            case '[':
                return Instruction::Jz;
            case ']':
                return Instruction::Jnz;
            case '+':
                return Instruction::Inc;
            case '-':
                return Instruction::Dec;
            case '<':
                return Instruction::Prev;
            case '>':
                return Instruction::Next;
            case '.':
                return Instruction::WriteChar;
            case ',':
                return Instruction::ReadChar;
            default:
                break;
            }
        }

        return Instruction::Invalid;
    }

private:
    Iterator m_first;
    Iterator m_last;
};
} // namespace bfjit

#endif // BFJIT_INSTRUCTION_HPP
