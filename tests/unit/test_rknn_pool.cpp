#include "catch_amalgamated.hpp"
#include "postprocess.h"
#include "rknnPool.hpp"
#include <opencv2/core.hpp>
#include <thread>
#include <atomic>
#include <vector>

/* Mock model that mimics rknnPool's required interface without hardware */
class MockModel {
    std::string path_;
    static std::atomic<int> global_delay_ms_;
public:
    MockModel(const char *path) : path_(path) {}

    int rknn_init(rknn_context *, bool, int) { return 0; }
    rknn_context *get_pctx() { static rknn_context ctx = 0; return &ctx; }

    cv::Mat infer(cv::Mat &img, detect_result_group_t *out) {
        int delay = global_delay_ms_.load();
        if (delay > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        if (out) { out->count = 0; out->id = 0; }
        return img;
    }

    detect_result_group_t getLastDetectResult() const {
        detect_result_group_t r;
        memset(&r, 0, sizeof(r));
        return r;
    }

    void setThresholds(float, float) {}

    static void setGlobalDelay(int ms) { global_delay_ms_ = ms; }
};
std::atomic<int> MockModel::global_delay_ms_{0};

using TestPool = rknnPool<MockModel, cv::Mat, cv::Mat>;

TEST_CASE("rknnPool - construction and init", "[pool]") {
    TestPool pool("test_model", 3);
    REQUIRE(pool.init() == 0);
}

TEST_CASE("rknnPool - put then get returns result in order", "[pool]") {
    TestPool pool("test_model", 3);
    REQUIRE(pool.init() == 0);

    cv::Mat f1(10, 10, CV_8UC3, cv::Scalar(1, 2, 3));
    cv::Mat f2(10, 10, CV_8UC3, cv::Scalar(4, 5, 6));
    cv::Mat f3(10, 10, CV_8UC3, cv::Scalar(7, 8, 9));

    REQUIRE(pool.put(f1) == 0);
    REQUIRE(pool.put(f2) == 0);
    REQUIRE(pool.put(f3) == 0);

    /* first 3 puts fill the idle threads, no get needed for warmup */
    cv::Mat out;
    REQUIRE(pool.get(out) == 0);

    REQUIRE(pool.get(out) == 0);

    REQUIRE(pool.get(out) == 0);

    REQUIRE(pool.get(out) == 1);  /* queue empty */
}

TEST_CASE("rknnPool - get on empty queue returns 1", "[pool]") {
    TestPool pool("test_model", 3);
    REQUIRE(pool.init() == 0);
    cv::Mat out;
    REQUIRE(pool.get(out) == 1);
}

TEST_CASE("rknnPool - queue overflow discards oldest frame when full", "[pool]") {
    /* Slow down inference so queue builds up */
    MockModel::setGlobalDelay(5);

    TestPool pool("test_model", 2);
    REQUIRE(pool.init() == 0);

    /* Push more than MAX_QUEUE_SIZE (16) frames */
    for (int i = 0; i < 30; i++) {
        cv::Mat f(10, 10, CV_8UC3, cv::Scalar(i, 0, 0));
        pool.put(f);
    }

    /* Drain all results */
    cv::Mat out;
    int count = 0;
    while (pool.get(out) == 0) count++;

    /* Queue should cap at MAX_QUEUE_SIZE, discarding oldest */
    REQUIRE(count <= 16);
    REQUIRE(count > 0);

    MockModel::setGlobalDelay(0);
}

TEST_CASE("rknnPool - multi-threaded put/get", "[pool]") {
    MockModel::setGlobalDelay(1);
    TestPool pool("test_model", 4);
    REQUIRE(pool.init() == 0);

    /* Keep N small enough to avoid queue overflow (MAX_QUEUE_SIZE=16) */
    constexpr int N = 12;
    std::atomic<int> total_got{0};

    std::thread putter([&]() {
        for (int i = 0; i < N; i++) {
            cv::Mat f(2, 2, CV_8UC3);
            pool.put(f);
        }
    });

    std::thread getter([&]() {
        cv::Mat out;
        while (total_got.load() < N) {
            if (pool.get(out) == 0) total_got++;
            else std::this_thread::yield();
        }
    });

    putter.join();
    getter.join();
    REQUIRE(total_got.load() == N);
    MockModel::setGlobalDelay(0);
}

TEST_CASE("rknnPool - set_thresholds does not crash", "[pool]") {
    TestPool pool("test_model", 3);
    REQUIRE(pool.init() == 0);
    REQUIRE_NOTHROW(pool.set_thresholds(0.5f, 0.6f));
}

TEST_CASE("rknnPool - getLastDetectResult after put/get returns empty", "[pool]") {
    TestPool pool("test_model", 3);
    REQUIRE(pool.init() == 0);

    cv::Mat f(10, 10, CV_8UC3);
    pool.put(f);
    cv::Mat out;
    pool.get(out);

    auto result = pool.getLastDetectResult();
    REQUIRE(result.count == 0);
}
