#include "arguments.hpp"
#include "mir_compiler.hpp"

#include <fmt/format.h>

#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>

using namespace bfjit;

struct Arguments
{
    String FileName;
    UInt32 HeapSize = 1 << 20;
};

Result WriteChar(CharType c)
{
    if (!(std::cout.put(c)))
    {
        return Result::WriteError;
    }
    else
    {
        return Result::Success;
    }
}

Result ReadChar(CharPtr target)
{
    assert(target);

    CharType c;
    if (!(std::cin.get(c)))
    {
        return Result::ReadError;
    }
    else
    {
        *target = c;
        return Result::Success;
    }
}

template <typename Iterator> Arguments ParseArguments(Iterator first, Iterator last)
{
    Arguments args;
    cli::ParseArguments(
        first, last,
        cli::Argument(args.FileName).WithDescription("The path to a file containing brainfuck sources").Required(),
        cli::Argument(args.HeapSize)
            .WithName("--heap-size")
            .WithDescription("The size of heap, in bytes, available to the VM")
            .WithDefaultValue("1048576"));

    return args;
}

Result RunFile(const String &sourceFile, UInt32 heapSize)
{
    std::ifstream sourceFileStream;
    sourceFileStream.open(sourceFile, std::ios::in);
    if (!sourceFileStream.is_open())
    {
        throw Exception::Formatted("failed to open source file {}", sourceFile);
    }

    MirCompiler compiler;
    IteratorInstructionReader reader(std::istream_iterator<char>{sourceFileStream}, std::istream_iterator<char>{});
    CompilerContext compilerContext{
        .WriteChar = WriteChar,
        .ReadChar = ReadChar,
        .Reader = &reader,
    };
    const auto entrypoint = compiler.Compile(compilerContext);

    Vector<CharType> heap(heapSize);
    return entrypoint(heap.data(), heap.data() + heap.size());
}

int main(int argc, const char **argv)
{
    try
    {
        const auto arguments = ParseArguments(argv + 1, argv + argc);
        try
        {
            return static_cast<int>(RunFile(arguments.FileName, arguments.HeapSize));
        }
        catch (Exception &ex)
        {
            fmt::print("failed to compile/run program: {}\n", ex.reason());
        }
    }
    catch (Exception &ex)
    {
        fmt::print("failed to parse command line arguments: {}\n", ex.reason());
    }

    return 1;
}