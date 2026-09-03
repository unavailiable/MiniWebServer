#ifndef EPOLLER_HPP
#define EPOLLER_HPP

#include <cstdint>
#include <sys/epoll.h>

#include "unique_fd.hpp"

class Epoller
{
  public:
    Epoller();

    ~Epoller() = default;

    Epoller(const Epoller&) = delete;
    Epoller& operator=(const Epoller&) = delete;

  public:
    bool add(int fd, uint32_t events, bool oneShot = false);

    bool modify(int fd, uint32_t events, bool oneShot = true);

    bool remove(int fd);

    int wait(epoll_event* events, int maxEvents, int timeout = -1);

  private:
    UniqueFd m_epollFd;
};

#endif