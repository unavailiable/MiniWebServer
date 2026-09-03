#include "../../include/core/unique_fd.hpp"

#include <unistd.h>

UniqueFd::UniqueFd() noexcept : m_fd(-1) {}

UniqueFd::UniqueFd(int fd) noexcept : m_fd(fd) {}

UniqueFd::~UniqueFd()
{
    reset();
}

UniqueFd::UniqueFd(UniqueFd&& other) noexcept : m_fd(other.release()) {}

UniqueFd& UniqueFd::operator=(UniqueFd&& other) noexcept
{
    if (this != &other)
    {
        reset(other.release());
    }

    return *this;
}

int UniqueFd::get() const noexcept
{
    return m_fd;
}

bool UniqueFd::valid() const noexcept
{
    return m_fd >= 0;
}

UniqueFd::operator bool() const noexcept
{
    return valid();
}

int UniqueFd::release() noexcept
{
    int fd = m_fd;
    m_fd = -1;

    return fd;
}

void UniqueFd::reset(int fd) noexcept
{
    if (m_fd >= 0)
    {
        close(m_fd);
    }

    m_fd = fd;
}