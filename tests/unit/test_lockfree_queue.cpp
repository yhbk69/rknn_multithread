/*
 * test_lockfree_queue.cpp - LockFreeQueue 并发压测
 *
 * 验证无锁 MPMC 队列在多线程高并发下的正确性：
 *   - 多生产者/多消费者不丢失数据
 *   - 每个元素恰好被消费一次
 *   - 无内存泄漏（shared_ptr 自动回收）
 */
#include "catch_amalgamated.hpp"
#include "LockFreeQueue.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <numeric>
#include <set>
#include <mutex>

TEST_CASE("LockFreeQueue - basic push/pop", "[lfq]") {
    lfq::LockFreeQueue<int> q;

    REQUIRE(q.empty());
    REQUIRE(!q.pop().has_value());

    q.push(std::make_shared<int>(42));
    REQUIRE(!q.empty());

    auto val = q.pop();
    REQUIRE(val.has_value());
    REQUIRE(**val == 42);
    REQUIRE(q.empty());
}

TEST_CASE("LockFreeQueue - FIFO order", "[lfq]") {
    lfq::LockFreeQueue<int> q;

    for (int i = 0; i < 100; i++)
        q.push(std::make_shared<int>(i));

    for (int i = 0; i < 100; i++) {
        auto val = q.pop();
        REQUIRE(val.has_value());
        REQUIRE(**val == i);
    }
    REQUIRE(q.empty());
}

TEST_CASE("LockFreeQueue - MPMC stress (4P/4C, 10k items)", "[lfq][stress]") {
    constexpr int PRODUCERS = 4;
    constexpr int CONSUMERS = 4;
    constexpr int ITEMS_PER_PRODUCER = 10000;
    constexpr int TOTAL = PRODUCERS * ITEMS_PER_PRODUCER;

    lfq::LockFreeQueue<int> q;
    std::atomic<int> consumed{0};
    std::vector<int> results;
    std::mutex result_mtx;

    // 生产者线程
    auto producer = [&](int base) {
        for (int i = 0; i < ITEMS_PER_PRODUCER; i++)
            q.push(std::make_shared<int>(base + i));
    };

    // 消费者线程
    std::atomic<bool> done{false};
    auto consumer = [&]() {
        std::vector<int> local;
        while (!done.load() || !q.empty()) {
            auto val = q.pop();
            if (val) {
                local.push_back(**val);
                consumed.fetch_add(1, std::memory_order_relaxed);
            }
        }
        // 消费剩余
        while (auto val = q.pop()) {
            local.push_back(**val);
            consumed.fetch_add(1, std::memory_order_relaxed);
        }
        std::lock_guard<std::mutex> lk(result_mtx);
        results.insert(results.end(), local.begin(), local.end());
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < PRODUCERS; i++)
        threads.emplace_back(producer, i * ITEMS_PER_PRODUCER);
    for (int i = 0; i < CONSUMERS; i++)
        threads.emplace_back(consumer);

    // 先等生产者完成，再通知消费者退出
    for (int i = 0; i < PRODUCERS; i++)
        threads[i].join();
    done = true;
    for (int i = PRODUCERS; i < PRODUCERS + CONSUMERS; i++)
        threads[i].join();

    REQUIRE(consumed.load() == TOTAL);
    REQUIRE((int)results.size() == TOTAL);

    // 验证每个值恰好出现一次
    std::set<int> unique(results.begin(), results.end());
    REQUIRE((int)unique.size() == TOTAL);

    // 验证值范围 [0, TOTAL)
    REQUIRE(*unique.begin() == 0);
    REQUIRE(*unique.rbegin() == TOTAL - 1);
}

TEST_CASE("LockFreeQueue - size_approx after push", "[lfq]") {
    lfq::LockFreeQueue<int> q;
    constexpr int N = 100;

    for (int i = 0; i < N; i++)
        q.push(std::make_shared<int>(i));

    // 推入 N 个元素后，size_approx 应接近 N
    size_t sz = q.size_approx();
    REQUIRE(sz == (size_t)N);

    // 逐个弹出
    for (int i = 0; i < N; i++) {
        auto val = q.pop();
        REQUIRE(val.has_value());
        REQUIRE(**val == i);
    }
    REQUIRE(q.empty());
}
