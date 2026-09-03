#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <condition_variable>
#include <thread>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>
#include <vector>
#include <atomic>

class threadpool
{
  public:
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    threadpool(const threadpool&) = delete;
    threadpool& operator=(const threadpool&) = delete;
    bool append(std::function<void()> request);

  private:
    // 线程池中每个线程的工作函数
    void run();

  private:
    // 线程的数量
    int m_thread_number;

    std::vector<std::thread> m_threads;
    // 请求队列中最多允许的、等待处理的请求的数量
    long unsigned int m_max_requests;

    // 请求队列
    std::queue<std::function<void()>> m_workqueue;

    // 保护请求队列的互斥锁
    std::mutex m_queueMtx;

    // 是否有任务需要处理
    std::condition_variable m_queuestat;

    // 是否结束线程
    std::atomic<bool> m_stop;
};

/**
 * 线程池类的构造函数
 * @param thread_number 线程池中线程的数量
 * @param max_requests 线程池中最大请求数量
 */
#endif