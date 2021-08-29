#ifndef BFJIT_ARGUMENTS_HPP
#define BFJIT_ARGUMENTS_HPP

#include "exception.hpp"

#include <cassert>
#include <charconv>
#include <tuple>
#include <type_traits>

namespace bfjit
{
namespace cli
{
namespace detail
{
template <typename T> struct IsOptional
{
    static constexpr inline bool Value = false;
};

template <typename T> struct IsOptional<std::optional<T>>
{
    static constexpr inline bool Value = true;
};

template <typename Type> struct ArgumentState
{
    using ArgumentType = Type;

    ArgumentType &Argument;
    bool Processed;
};

inline void SetValue(String &target, const StringRef &source)
{
    target = source;
}

inline void SetValue(UInt32 &target, const StringRef &source)
{
    const auto first = source.data();
    const auto last = first + source.size();
    if (std::from_chars(first, last, target).ec != std::errc())
    {
        throw Exception::Formatted("failed to convert value `{}` into uint32", source);
    }
}

template <typename T> inline void SetValue(std::optional<T> &target, const StringRef &source)
{
    SetValue(target.emplace(), source);
}

template <typename Argument> void ValidateArgument(const ArgumentState<Argument> &state)
{
    using ValueType = typename Argument::ValueType;

    if constexpr (!IsOptional<ValueType>::Value)
    {
        if (!state.Processed)
        {
            throw Exception::Formatted("missing command line argument {}", state.Argument.Name);
        }
    }
}

template <typename... States> void ValidateArguments(const std::tuple<States...> &states)
{
    const auto ValidateArgumentsImpl = [](const auto &...states) { (ValidateArgument(states), ...); };
    std::apply(ValidateArgumentsImpl, states);
}

template <typename... Arguments> auto CreateStates(Arguments &...arguments)
{
    const auto CreateState = [](auto &argument) { return ArgumentState{.Argument = argument, .Processed = false}; };
    return std::make_tuple(CreateState(arguments)...);
}

template <typename Iterator, typename... States>
void ParseArguments(Iterator first, Iterator last, std::tuple<States...> &states)
{
    auto ParseOption = [&](const StringRef &currentArgument, auto &state) -> bool {
        if (!state.Processed && !state.Argument.Name.empty() && state.Argument.Name == currentArgument)
        {
            if (++first == last)
            {
                throw Exception::Formatted("missing command line argument value {}", state.Argument.Name);
            }

            state.Processed = true;
            SetValue(state.Argument.Value, *first++);

            return true;
        }

        return false;
    };

    auto ParseArgument = [](const StringRef &currentArgument, auto &state) -> bool {
        if (!state.Processed && state.Argument.Name.empty())
        {
            state.Processed = true;
            SetValue(state.Argument.Value, currentArgument);
            return true;
        }

        return false;
    };

    while (first != last)
    {
        const auto currentArgument = StringRef(*first++);
        auto ParseArgumentsImpl = [&](auto &...states) {
            if (!(ParseOption(currentArgument, states) || ...))
            {
                if (currentArgument.starts_with("-"))
                {
                    throw Exception::Formatted("unsupported argument {}", currentArgument);
                }

                return (ParseArgument(currentArgument, states) || ...);
            }

            return true;
        };

        if (!std::apply(ParseArgumentsImpl, states))
        {
            throw Exception::Formatted("unexpected command line argument {}", *first);
        }
    }
}
} // namespace detail

template <typename Type> struct Argument
{
    using ValueType = Type;

    StringRef Name;
    ValueType &Value;

    explicit Argument(ValueType &value) noexcept : Value(value)
    {
    }

    explicit Argument(const StringRef &name, ValueType &value) noexcept : Name(name), Value(value)
    {
    }
};

template <typename Iterator, typename... Arguments>
void ParseArguments(Iterator first, Iterator last, Arguments &&...arguments)
{
    auto states = detail::CreateStates(arguments...);
    detail::ParseArguments(first, last, states);
    detail::ValidateArguments(states);
}
} // namespace cli
} // namespace bfjit

#endif // BFJIT_ARGUMENTS_HPP
