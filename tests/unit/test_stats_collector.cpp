#include "catch_amalgamated.hpp"
#include "pipeline/StatsCollector.hpp"
#include <thread>

TEST_CASE("StatsCollector - initial state", "[stats]") {
    StatsCollector stats;
    REQUIRE(stats.getCurrentFps() == 0.0);
    REQUIRE(stats.calcAvgFps(0) == 0.0);
}

TEST_CASE("StatsCollector - elapsed time after start", "[stats]") {
    StatsCollector stats;
    stats.start();
    REQUIRE(stats.elapsedMs() < 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(stats.elapsedMs() >= 8);
}

TEST_CASE("StatsCollector - updateFps before start returns 0", "[stats]") {
    StatsCollector stats;
    REQUIRE(stats.updateFps(0) == 0.0);
    REQUIRE(stats.updateFps(1) == 0.0);
}

TEST_CASE("StatsCollector - updateFps after start", "[stats]") {
    StatsCollector stats;
    stats.start();
    REQUIRE(stats.updateFps(5) == 0.0);
    REQUIRE(stats.updateFps(30) >= 0.0);
    REQUIRE(stats.getCurrentFps() >= 0.0);
}

TEST_CASE("StatsCollector - calcAvgFps", "[stats]") {
    StatsCollector stats;
    stats.start();
    REQUIRE(stats.calcAvgFps(0) == 0.0);
    REQUIRE(stats.calcAvgFps(100) >= 0.0);
}

TEST_CASE("StatsCollector - monotonic clock", "[stats]") {
    StatsCollector stats;
    auto before = std::chrono::steady_clock::now();
    stats.start();
    auto after = std::chrono::steady_clock::now();
    REQUIRE(stats.elapsedMs() >= 0);
    REQUIRE(stats.elapsedMs() <= 100);
}

TEST_CASE("StatsCollector - updateFps with interval", "[stats]") {
    StatsCollector stats;
    stats.start();
    double fps1 = stats.updateFps(30, 30);
    double fps2 = stats.updateFps(60, 30);
    REQUIRE(fps1 >= 0.0);
    REQUIRE(fps2 >= 0.0);
}

TEST_CASE("StatsCollector - elapsed precision", "[stats]") {
    StatsCollector stats;
    stats.start();
    auto t1 = stats.elapsedMs();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    auto t2 = stats.elapsedMs();
    REQUIRE(t2 >= t1);
}
