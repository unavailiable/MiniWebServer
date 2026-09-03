#include "../../include/core/epoller.hpp"

#include <cerrno>
#include <system_error>

Epoller::Epoller()
{
    int fd = epoll_create1(EPOLL_CLOEXEC);

    if (fd < 0)
    {
        throw std::system_error(errno, std::generic_category(), "epoll_create1");
    }

    m_epollFd.reset(fd);
}

bool Epoller::add(int fd, uint32_t events, bool oneShot)
{
    epoll_event event{};

    event.data.fd = fd;

    event.events = events;

    if (oneShot)
    {
        event.events |= EPOLLONESHOT;
    }

    return epoll_ctl(m_epollFd.get(), EPOLL_CTL_ADD, fd, &event) == 0;
}

bool Epoller::modify(int fd, uint32_t events, bool oneShot)
{
    epoll_event event{};

    event.data.fd = fd;

    event.events = events;

    if (oneShot)
    {
        event.events |= EPOLLONESHOT;
    }

    return epoll_ctl(m_epollFd.get(), EPOLL_CTL_MOD, fd, &event) == 0;
}

bool Epoller::remove(int fd)
{
    return epoll_ctl(m_epollFd.get(), EPOLL_CTL_DEL, fd, nullptr) == 0;
}

int Epoller::wait(epoll_event* events, int maxEvents, int timeout)
{
    return epoll_wait(m_epollFd.get(), events, maxEvents, timeout);
}