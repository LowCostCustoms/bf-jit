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
template <typename Type> struct ArgumentState
{
    using ArgumentType = Type;

    ArgumentType &Argument;
    Boolean IsProcessed;
};

template <typename Argument> void PostProcessArgument(const ArgumentState<Argument> &state)
{
    if (!state.IsProcessed && state.Argument.DefaultValue) {
        state.Argument.Value = state.Argument.Parser(*state.Argument.DefaultValue);
        return;
    }

    if (state.Argument.IsRequired && !state.IsProcessed)
    {
        throw Exception::Formatted("missing command line argument `{}`", state.Argument.Name);
    }
}

template <typename... States> void PostProcessArguments(const std::tuple<States...> &states)
{
    const auto PostProcessImpl = [](const auto &...states) { (PostProcessArgument(states), ...); };
    std::apply(PostProcessImpl, states);
}

template <typename... Arguments> auto CreateStates(Arguments &...arguments)
{
    const auto CreateState = [](auto &argument) { return ArgumentState{.Argument = argument, .IsProcessed = false}; };
    return std::make_tuple(CreateState(arguments)...);
}

template <typename Iterator, typename... States>
void ParseArguments(Iterator first, Iterator last, std::tuple<States...> &states)
{
    auto ParseOption = [&](const StringRef &currentArgument, auto &state) -> bool {
        if (!state.IsProcessed && !state.Argument.Name.empty() && state.Argument.Name == currentArgument)
        {
            if (first == last)
            {
                throw Exception::Formatted("missing command line argument value `{}`", state.Argument.Name);
            }

            state.IsProcessed = true;
            state.Argument.Value = state.Argument.Parser(StringRef(*first++));

            return true;
        }

        return false;
    };

    auto ParseArgument = [](const StringRef &currentArgument, auto &state) -> bool {
        if (!state.IsProcessed && state.Argument.Name.empty())
        {
            state.IsProcessed = true;
            state.Argument.Value = state.Argument.Parser(currentArgument);
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
                    throw Exception::Formatted("unsupported argument `{}`", currentArgument);
                }

                return (ParseArgument(currentArgument, states) || ...);
            }

            return true;
        };

        if (!std::apply(ParseArgumentsImpl, states))
        {
            throw Exception::Formatted("unexpected command line argument `{}`", currentArgument);
        }
    }
}
} // namespace detail

template <typename T> struct DefaultParser;

template <> struct DefaultParser<String>
{
    String operator()(const StringRef &value) const
    {
        return String(value);
    }
};

template <> struct DefaultParser<UInt32>
{
    UInt32 operator()(const StringRef &value) const
    {
        UInt32 result = 0;
        const auto first = value.data();
        const auto last = first + value.size();
        if (std::from_chars(first, last, result).ec != std::errc())
        {
            throw Exception::Formatted("failed to parse `{}` to uint32", value);
        }

        return result;
    }
};

template <typename Type, typename Parser_ = DefaultParser<Type>> struct Argument
{
    using ValueType = Type;
    using ParserType = Parser_;
    using SelfType = Argument<Type, Parser_>;

    StringRef Name;
    Optional<StringRef> Description;
    Optional<StringRef> DefaultValue;
    Boolean IsRequired = false;
    ValueType &Value;
    ParserType Parser;

    explicit Argument(ValueType &value, const ParserType &parser = ParserType()) noexcept : Value(value)
    {
    }

    SelfType &WithName(const StringRef &name) noexcept
    {
        Name = name;
        return *this;
    }

    SelfType &WithDescription(const StringRef &description) noexcept
    {
        Description = description;
        return *this;
    }

    SelfType &Required() noexcept
    {
        IsRequired = true;
        return *this;
    }

    SelfType &WithDefaultValue(const StringRef &value) noexcept {
        DefaultValue = value;
        return *this;
    }
};

template <typename Iterator, typename... Arguments>
void ParseArguments(Iterator first, Iterator last, Arguments &&...arguments)
{
    auto states = detail::CreateStates(arguments...);
    detail::ParseArguments(first, last, states);
    detail::PostProcessArguments(states);
}
} // namespace cli
} // namespace bfjit

#endif // BFJIT_ARGUMENTS_HPP
