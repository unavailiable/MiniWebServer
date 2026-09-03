#ifndef SERVER_HPP
#define SERVER_HPP

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

#include "epoller.hpp"
#include "listener.hpp"
#include "thread_pool.hpp"
#include "../http/http_connection.hpp"

class Server
{
  public:
    Server(int port, const std::string& docRoot, int threadNumber = 8, int maxRequests = 10000);

    ~Server();

    Server(const Server&) = delete;

    Server& operator=(const Server&) = delete;

  public:
    void run();

    void stop();

  private:
    void handleAccept();

    void handleRead(int fd);

    void handleWrite(int fd);

    void handleError(int fd);

    void dispatch(const std::shared_ptr<HttpConnection>& connection);

    void closeConnection(int fd);

  private:
    static std::string canonicalizeRoot(const std::string& root);

  private:
    static constexpr int MAX_EVENTS = 10000;

  private:
    Listener m_listener;

    Epoller m_epoller;

    std::string m_docRoot;

    std::unordered_map<int, std::shared_ptr<HttpConnection>> m_connections;

    std::atomic<bool> m_running;

    /*
     * 有意放最后。
     *
     * C++ 成员按声明逆序析构。
     *
     * 所以 Server 析构时：
     *
     * ThreadPool 最先析构
     * -> join 所有 worker
     * -> 然后 connections 才析构
     */
    threadpool m_threadPool;
};

#endif