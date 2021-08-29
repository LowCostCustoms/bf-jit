#include "exception.hpp"

#include <cassert>

using namespace bfjit;

struct Exception::Impl
{
    std::string Message;

    template <typename T> explicit Impl(T &&message) : Message(std::forward<T>(message))
    {
    }
};

Exception::Exception(const String &reason) : m_impl(std::make_shared<Impl>(reason))
{
}

Exception::Exception(String &&reason) : m_impl(std::make_shared<Impl>(std::move(reason)))
{
}

const char *Exception::what() const noexcept
{
    assert(m_impl);

    return m_impl->Message.c_str();
}

StringRef Exception::reason() const noexcept
{
    assert(m_impl);

    return m_impl->Message;
}
