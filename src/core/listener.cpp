#include "../../include/core/listener.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

Listener::Listener(int port, int backlog) : m_port(port), m_backlog(backlog)
{
    if (port <= 0 || port > 65535 || backlog <= 0)
    {
        throw std::invalid_argument("invalid listener configuration");
    }

    createSocket();

    setSocketOptions();

    bindAddress();

    startListen();
}

void Listener::createSocket()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0)
    {
        throw std::system_error(errno, std::generic_category(), "socket");
    }

    m_listenFd.reset(fd);

    if (!setNonBlocking(fd))
    {
        throw std::system_error(errno, std::generic_category(), "set listen fd nonblocking");
    }
}

void Listener::setSocketOptions()
{
    int reuse = 1;

    if (setsockopt(m_listenFd.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "setsockopt");
    }
}

void Listener::bindAddress()
{
    sockaddr_in address;

    std::memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;

    address.sin_addr.s_addr = htonl(INADDR_ANY);

    address.sin_port = htons(m_port);

    if (bind(m_listenFd.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "bind");
    }
}

void Listener::startListen()
{
    if (listen(m_listenFd.get(), m_backlog) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "listen");
    }
}

int Listener::fd() const noexcept
{
    return m_listenFd.get();
}

int Listener::accept(sockaddr_in& clientAddress)
{
    socklen_t length = sizeof(clientAddress);

    int clientFd = ::accept(m_listenFd.get(), reinterpret_cast<sockaddr*>(&clientAddress), &length);

    if (clientFd < 0)
    {
        return -1;
    }

    if (!setNonBlocking(clientFd))
    {
        close(clientFd);
        return -1;
    }

    return clientFd;
}

bool Listener::setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags < 0)
    {
        return false;
    }

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}