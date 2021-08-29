#ifndef BFJIT_EXCEPTION_HPP
#define BFJIT_EXCEPTION_HPP

#include "types.hpp"

#include <fmt/format.h>

#include <exception>
#include <memory>
#include <string>

namespace bfjit
{
class Exception : public std::exception
{
public:
    template <typename FormatString, typename... Args> static Exception Formatted(FormatString &&format, Args &&...args)
    {
        return Exception(fmt::format(format, args...));
    }

    explicit Exception(const String &reason);

    explicit Exception(String &&reason);

    const char *what() const noexcept override;

    StringRef reason() const noexcept;

private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;
};
} // namespace bfjit

#endif // BFJIT_EXCEPTION_HPP
