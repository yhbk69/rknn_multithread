/*
 * ThreadPool.hpp
 * 第三方线程池库 dpool 的头文件
 * 来源：https://github.com/senlinzhan/dpool
 * 支持动态线程创建和空闲线程超时回收，按需创建线程而非预创建，
 * 当线程空闲超过指定时间后自动退出以释放系统资源。
 */
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <cassert>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

// dpool 命名空间：包含线程池相关的类和工具
namespace dpool
{

    // ThreadPool: 动态线程池类
    // 按需创建工作线程，最大线程数可配置；
    // 支持空闲线程超时自动回收，减少资源占用；
    // 线程安全，支持多线程并发提交任务。
    class ThreadPool
    {
    public:
        using MutexGuard = std::lock_guard<std::mutex>;   // 互斥锁守卫（RAII 风格加锁/解锁）
        using UniqueLock = std::unique_lock<std::mutex>;   // 唯一锁（支持条件变量等待/唤醒）
        using Thread = std::thread;                         // 线程类型别名
        using ThreadID = std::thread::id;                  // 线程 ID 类型别名
        using Task = std::function<void()>;                // 任务类型：无参数无返回值的可调用对象

        // 默认构造函数：最大线程数设为硬件并发数
        ThreadPool()
            : ThreadPool(Thread::hardware_concurrency())
        {
        }

        // 显式构造函数：指定线程池最大线程数
        explicit ThreadPool(size_t maxThreads)
            : quit_(false),
              currentThreads_(0),
              idleThreads_(0),
              maxThreads_(maxThreads)
        {
        }

        // 禁用拷贝操作，防止线程池被复制导致资源管理混乱
        ThreadPool(const ThreadPool &) = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;

        // 析构函数：通知所有工作线程退出，并等待所有线程结束
        ~ThreadPool()
        {
            {
                MutexGuard guard(mutex_);
                quit_ = true;
            }
            cv_.notify_all();

            for (auto &elem : threads_)
            {
                assert(elem.second.joinable());
                elem.second.join();
            }
        }

        // 提交任务到线程池，返回 std::future 用于获取异步结果
        // 如果有空闲线程则唤醒一个执行；否则若未达最大线程数则创建新线程；
        // 若已达上限则任务排队等待空闲线程处理。
        template <typename Func, typename... Ts>
        auto submit(Func &&func, Ts &&...params)
            -> std::future<typename std::result_of<Func(Ts...)>::type>
        {
            auto execute = std::bind(std::forward<Func>(func), std::forward<Ts>(params)...);

            using ReturnType = typename std::result_of<Func(Ts...)>::type;
            using PackagedTask = std::packaged_task<ReturnType()>;

            auto task = std::make_shared<PackagedTask>(std::move(execute));
            auto result = task->get_future();

            MutexGuard guard(mutex_);
            assert(!quit_);

            tasks_.emplace([task]()
                           { (*task)(); });
            if (idleThreads_ > 0)
            {
                cv_.notify_one();
            }
            else if (currentThreads_ < maxThreads_)
            {
                Thread t(&ThreadPool::worker, this);
                assert(threads_.find(t.get_id()) == threads_.end());
                threads_[t.get_id()] = std::move(t);
                ++currentThreads_;
            }

            return result;
        }

        // 获取当前活跃线程数（包括正在执行任务和空闲等待的线程）
        size_t threadsNum() const
        {
            MutexGuard guard(mutex_);
            return currentThreads_;
        }

    private:
        // 工作线程主函数
        // 循环从任务队列取任务执行：
        // - 有新任务时取出并执行
        // - 无任务时等待，空闲超过 WAIT_SECONDS 秒则自动退出以回收资源
        // - 收到退出通知且无剩余任务时退出
        void worker()
        {
            while (true)
            {
                Task task;
                {
                    UniqueLock uniqueLock(mutex_);
                    ++idleThreads_;
                    auto hasTimedout = !cv_.wait_for(uniqueLock,
                                                     std::chrono::seconds(WAIT_SECONDS),
                                                     [this]()
                                                     {
                                                         return quit_ || !tasks_.empty();
                                                     });
                    --idleThreads_;
                    if (tasks_.empty())
                    {
                        if (quit_)
                        {
                            --currentThreads_;
                            return;
                        }
                        if (hasTimedout)
                        {
                            --currentThreads_;
                            joinFinishedThreads();
                            finishedThreadIDs_.emplace(std::this_thread::get_id());
                            return;
                        }
                    }
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                task();
            }
        }

        // 回收已结束的线程资源，遍历已结束线程 ID 队列，逐一 join 并从线程表中移除
        void joinFinishedThreads()
        {
            while (!finishedThreadIDs_.empty())
            {
                auto id = std::move(finishedThreadIDs_.front());
                finishedThreadIDs_.pop();
                auto iter = threads_.find(id);

                assert(iter != threads_.end());
                assert(iter->second.joinable());

                iter->second.join();
                threads_.erase(iter);
            }
        }

        // 空闲线程超时时间（单位：秒），超过此时间无任务可执行的工作线程将自动退出
        static constexpr size_t WAIT_SECONDS = 2;

        bool quit_;                // 线程池是否已关闭（析构时设为 true）
        size_t currentThreads_;    // 当前活跃线程数量
        size_t idleThreads_;       // 当前空闲（等待任务）的线程数量
        size_t maxThreads_;        // 线程池允许的最大线程数

        mutable std::mutex mutex_;                          // 保护共享数据的互斥锁
        std::condition_variable cv_;                        // 条件变量，用于通知工作线程有新任务或退出
        std::queue<Task> tasks_;                            // 待执行的任务队列
        std::queue<ThreadID> finishedThreadIDs_;            // 已结束线程的 ID 队列，等待回收
        std::unordered_map<ThreadID, Thread> threads_;      // 线程表：线程 ID -> 线程对象的映射
    };

    constexpr size_t ThreadPool::WAIT_SECONDS;

} // namespace dpool

#endif /* THREADPOOL_H */