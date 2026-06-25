/*
 * test_framepool.cpp - FramePool 并发压测
 *
 * 验证帧内存池在多线程高并发下的正确性：
 *   - acquire/release 不死锁
 *   - 池大小限制不被突破
 *   - 缓冲区数据不互相干扰
 */
#include "catch_amalgamated.hpp"
#include "FramePool.hpp"
#include <thread>
#include <vector>
#include <atomic>

TEST_CASE("FramePool - basic acquire/release", "[framepool]") {
    FramePool pool(4);

    cv::Mat m1 = pool.acquire(100, 200, CV_8UC3);
    REQUIRE(m1.rows == 100);
    REQUIRE(m1.cols == 200);

    pool.release(m1);

    // 重用：可能返回同一缓冲区
    cv::Mat m2 = pool.acquire(100, 200, CV_8UC3);
    REQUIRE(m2.rows == 100);
    pool.release(m2);
}

TEST_CASE("FramePool - size limit respected", "[framepool]") {
    constexpr size_t POOL_SIZE = 4;
    FramePool pool(POOL_SIZE);

    std::vector<cv::Mat> mats;
    for (int i = 0; i < 8; i++)
        mats.push_back(pool.acquire(64, 64, CV_8UC3));

    for (auto& m : mats)
        pool.release(m);

    auto st = pool.stats();
    // 池大小限制为 4，多余的应被丢弃
    REQUIRE(st.free_count <= POOL_SIZE);
}

TEST_CASE("FramePool - multi-thread stress (8T x 10k)", "[framepool]") {
    constexpr int THREADS = 8;
    constexpr int OPS_PER_THREAD = 10000;
    constexpr size_t POOL_SIZE = 16;

    FramePool pool(POOL_SIZE);
    std::atomic<int> acquire_count{0};
    std::atomic<int> release_count{0};
    std::atomic<bool> failed{false};

    auto worker = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            cv::Mat m = pool.acquire(480, 640, CV_8UC3);
            if (m.empty() || m.rows != 480 || m.cols != 640) {
                failed.store(true);
                continue;
            }
            m.setTo(cv::Scalar(i % 256, (i * 2) % 256, (i * 3) % 256));
            acquire_count.fetch_add(1, std::memory_order_relaxed);
            pool.release(m);
            release_count.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < THREADS; i++)
        threads.emplace_back(worker);

    for (auto& t : threads) t.join();

    REQUIRE_FALSE(failed.load());
    REQUIRE(acquire_count.load() == THREADS * OPS_PER_THREAD);
    REQUIRE(release_count.load() == THREADS * OPS_PER_THREAD);

    auto st = pool.stats();
    REQUIRE(st.free_count <= POOL_SIZE);
}

TEST_CASE("FramePool - mixed sizes concurrent", "[framepool]") {
    constexpr int THREADS = 4;
    constexpr int OPS = 5000;
    FramePool pool(8);

    struct Size { int rows, cols; };
    Size sizes[] = {{480, 640}, {1080, 1920}, {64, 64}, {240, 320}};

    std::atomic<int> ops{0};
    std::atomic<bool> failed{false};

    auto worker = [&](int tid) {
        for (int i = 0; i < OPS; i++) {
            auto& sz = sizes[(tid + i) % 4];
            cv::Mat m = pool.acquire(sz.rows, sz.cols, CV_8UC3);
            if (m.rows != sz.rows || m.cols != sz.cols) {
                failed.store(true);
                continue;
            }
            pool.release(m);
            ops.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < THREADS; i++)
        threads.emplace_back(worker, i);

    for (auto& t : threads) t.join();

    REQUIRE_FALSE(failed.load());
    REQUIRE(ops.load() == THREADS * OPS);
}
