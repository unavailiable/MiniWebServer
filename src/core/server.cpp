#include "../../include/core/server.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <limits.h>
#include <stdexcept>
#include <sys/epoll.h>
#include <system_error>

Server::Server(int port, const std::string& docRoot, int threadNumber, int maxRequests)
    : m_listener(port), m_epoller(), m_docRoot(canonicalizeRoot(docRoot)), m_running(false),
      m_threadPool(threadNumber, maxRequests)
{
    if (!m_epoller.add(m_listener.fd(), EPOLLIN, false))
    {
        throw std::runtime_error("failed to add listen fd to epoll");
    }
}

Server::~Server()
{
    stop();
}

std::string Server::canonicalizeRoot(const std::string& root)
{
    char realRoot[PATH_MAX];

    if (!realpath(root.c_str(), realRoot))
    {
        throw std::system_error(errno, std::generic_category(), "invalid document root");
    }

    return std::string(realRoot);
}

void Server::run()
{
    m_running = true;

    epoll_event events[MAX_EVENTS];

    while (m_running)
    {
        /*
         * 1 秒 timeout。
         *
         * v2 第一版暂时不用 eventfd
         * 唤醒 stop()。
         */
        int count = m_epoller.wait(events, MAX_EVENTS, 1000);

        if (count < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            throw std::system_error(errno, std::generic_category(), "epoll_wait");
        }

        for (int i = 0; i < count; ++i)
        {
            int fd = events[i].data.fd;

            uint32_t event = events[i].events;

            if (fd == m_listener.fd())
            {
                handleAccept();

                continue;
            }

            if (event & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            {
                handleError(fd);

                continue;
            }

            if (event & EPOLLIN)
            {
                handleRead(fd);

                continue;
            }

            if (event & EPOLLOUT)
            {
                handleWrite(fd);

                continue;
            }
        }
    }
}

void Server::stop()
{
    m_running = false;
}

void Server::handleAccept()
{
    while (true)
    {
        sockaddr_in clientAddress{};

        int clientFd = m_listener.accept(clientAddress);

        if (clientFd < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }

            std::cerr << "accept failed: " << std::strerror(errno) << std::endl;

            break;
        }

        std::shared_ptr<HttpConnection> connection =
            std::make_shared<HttpConnection>(clientFd, clientAddress, m_docRoot);

        m_connections.emplace(clientFd, connection);

        if (!m_epoller.add(clientFd, EPOLLIN | EPOLLRDHUP, true))
        {
            m_connections.erase(clientFd);

            continue;
        }
    }
}

void Server::handleRead(int fd)
{
    auto iterator = m_connections.find(fd);

    if (iterator == m_connections.end())
    {
        return;
    }

    std::shared_ptr<HttpConnection> connection = iterator->second;

    if (!connection->read())
    {
        closeConnection(fd);

        return;
    }

    dispatch(connection);
}

void Server::dispatch(const std::shared_ptr<HttpConnection>& connection)
{
    bool success = m_threadPool.append(
        [this, connection]
        {
            HttpConnection::ProcessResult result = connection->process();

            uint32_t events = EPOLLRDHUP;

            if (result == HttpConnection::ProcessResult::NeedMoreData)
            {
                events |= EPOLLIN;
            }
            else
            {
                events |= EPOLLOUT;
            }

            /*
             * epoll_ctl 本身允许不同线程调用。
             *
             * EPOLLONESHOT 保证这个连接
             * 当前不会被 Reactor 再次调度。
             */
            if (!m_epoller.modify(connection->fd(), events, true))
            {
                std::cerr << "failed to rearm fd " << connection->fd() << std::endl;
            }
        });

    if (!success)
    {
        /*
         * v2 第一阶段仍采用与
         * v1.0.1 相同的 fail-fast 策略。
         *
         * 后面再升级 backpressure。
         */
        closeConnection(connection->fd());
    }
}

void Server::handleWrite(int fd)
{
    auto iterator = m_connections.find(fd);

    if (iterator == m_connections.end())
    {
        return;
    }

    std::shared_ptr<HttpConnection> connection = iterator->second;

    HttpConnection::WriteResult result = connection->write();

    switch (result)
    {
    case HttpConnection::WriteResult::NeedWrite:
    {
        if (!m_epoller.modify(fd, EPOLLOUT | EPOLLRDHUP, true))
        {
            closeConnection(fd);
        }

        break;
    }

    case HttpConnection::WriteResult::NeedRead:
    {
        if (!m_epoller.modify(fd, EPOLLIN | EPOLLRDHUP, true))
        {
            closeConnection(fd);
        }

        break;
    }

    case HttpConnection::WriteResult::Close:
    {
        closeConnection(fd);

        break;
    }
    }
}

void Server::handleError(int fd)
{
    closeConnection(fd);
}

void Server::closeConnection(int fd)
{
    auto iterator = m_connections.find(fd);

    if (iterator == m_connections.end())
    {
        return;
    }

    m_epoller.remove(fd);

    /*
     * map 删除 shared_ptr。
     *
     * 如果此时 worker lambda
     * 仍然持有 connection，
     * HttpConnection 不会立即析构。
     *
     * worker 完成后最后一个 shared_ptr
     * 销毁，socket fd 才真正 close。
     */
    m_connections.erase(iterator);
}