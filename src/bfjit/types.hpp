#ifndef BFJIT_TYPES_HPP
#define BFJIT_TYPES_HPP

#include "instruction.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bfjit
{
using CharType = char;
using CharPtr = CharType *;
using String = std::string;
using StringRef = std::string_view;
using UInt32 = std::uint32_t;
using Boolean = bool;

template<typename T> using Optional = std::optional<T>;
template<typename T> using Vector = std::vector<T>;

enum class Result : UInt32
{
    Success,
    WriteError,
    ReadError,
    MemoryUnderrun,
    OutOfMemory,
};

using WriteCharFunc = Result (*)(CharType);
using ReadCharFunc = Result (*)(CharPtr);
using MainFunc = Result (*)(CharPtr, CharPtr);

struct CompilerContext
{
    WriteCharFunc WriteChar;
    ReadCharFunc ReadChar;
    InstructionReader *Reader;
};

struct CompilerBackend
{
    virtual ~CompilerBackend() = default;
    virtual MainFunc Compile(const CompilerContext &context) = 0;
};
} // namespace bfjit

#endif // BFJIT_TYPES_HPP
