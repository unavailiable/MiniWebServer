#ifndef LISTENER_HPP
#define LISTENER_HPP

#include <netinet/in.h>

#include "unique_fd.hpp"

class Listener
{
  public:
    explicit Listener(int port, int backlog = 128);

    ~Listener() = default;

    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;

  public:
    int fd() const noexcept;

    int accept(sockaddr_in& clientAddress);

  private:
    void createSocket();

    void setSocketOptions();

    void bindAddress();

    void startListen();

    static bool setNonBlocking(int fd);

  private:
    int m_port;

    int m_backlog;

    UniqueFd m_listenFd;
};

#endif