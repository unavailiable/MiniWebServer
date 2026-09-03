
#include <iostream>
#include "../../include/core/thread_pool.hpp"
threadpool::threadpool(int thread_number, int max_requests)
    : m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false)
{

    // 检查线程数量和最大请求数量是否合法
    if ((thread_number <= 0) || (max_requests <= 0))
    {
        throw std::invalid_argument("thread_number and max_requests must be positive");
    }

    // 创建thread_number 个线程
    m_threads.reserve(m_thread_number);
    for (int i = 0; i < thread_number; ++i)
    {
        std::cout << "create the " << i << "th thread" << std::endl;
        try
        {
            m_threads.emplace_back([this] { this->run(); });
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << std::endl;
            m_stop = true;
            m_queuestat.notify_all();
            for (std::thread& x : m_threads)
            {
                x.join();
            }
            throw;
        }
    }
}

threadpool::~threadpool()
{
    m_stop = true;
    m_queuestat.notify_all();

    for (std::thread& t : m_threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
}

bool threadpool::append(std::function<void()> request)
{

    // 操作工作队列时一定要加锁，因为它被所有线程共享。
    {
        std::lock_guard<std::mutex> lock(m_queueMtx);
        if (m_workqueue.size() >= m_max_requests)
        {
            return false;
        }
        m_workqueue.push(std::move(request));
    }
    m_queuestat.notify_one();
    return true;
}

void threadpool::run()
{
    while (true)
    {
        std::function<void()> request;
        {
            std::unique_lock<std::mutex> lock(m_queueMtx);
            m_queuestat.wait(lock, [this] { return this->m_stop || !this->m_workqueue.empty(); });

            if (m_stop)
                break;

            request = std::move(m_workqueue.front());
            m_workqueue.pop();
        }

        try
        {
            request();
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
}