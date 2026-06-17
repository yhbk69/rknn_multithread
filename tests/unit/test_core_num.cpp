#include "catch_amalgamated.hpp"
#include "coreNum.hpp"
#include <thread>
#include <vector>
#include <set>

TEST_CASE("coreNum - get_core_num cycles through 0,1,2", "[core]") {
    int a = get_core_num();
    int b = get_core_num();
    int c = get_core_num();
    int d = get_core_num();

    REQUIRE(a >= 0);
    REQUIRE(a < 3);
    REQUIRE(b >= 0);
    REQUIRE(b < 3);
    REQUIRE(c >= 0);
    REQUIRE(c < 3);
    REQUIRE(d >= 0);
    REQUIRE(d < 3);
    REQUIRE((a + 1) % 3 == b);
    REQUIRE((b + 1) % 3 == c);
    REQUIRE((c + 1) % 3 == d);
}

TEST_CASE("coreNum - get_core_for_channel with default instance", "[core]") {
    REQUIRE(get_core_for_channel(0) == 0);
    REQUIRE(get_core_for_channel(1) == 1);
    REQUIRE(get_core_for_channel(2) == 2);
    REQUIRE(get_core_for_channel(3) == 0);
    REQUIRE(get_core_for_channel(4) == 1);
}

TEST_CASE("coreNum - get_core_for_channel with instance offset", "[core]") {
    REQUIRE(get_core_for_channel(0, 0) == 0);
    REQUIRE(get_core_for_channel(0, 1) == 1);
    REQUIRE(get_core_for_channel(0, 2) == 2);
    REQUIRE(get_core_for_channel(0, 3) == 0);
    REQUIRE(get_core_for_channel(1, 0) == 1);
    REQUIRE(get_core_for_channel(1, 1) == 2);
    REQUIRE(get_core_for_channel(1, 2) == 0);
}

TEST_CASE("coreNum - get_core_num is thread-safe, all values in range", "[core]") {
    constexpr int N = 1000;
    std::vector<int> results(N);
    std::vector<std::thread> threads;

    for (int i = 0; i < 4; i++) {
        threads.emplace_back([i, &results, N]() {
            for (int j = i; j < N; j += 4)
                results[j] = get_core_num();
        });
    }
    for (auto &t : threads) t.join();

    for (int v : results) {
        REQUIRE(v >= 0);
        REQUIRE(v < 3);
    }
}

TEST_CASE("coreNum - channel distribution covers all cores evenly", "[core]") {
    std::set<int> cores;
    for (int ch = 0; ch < 100; ch++)
        cores.insert(get_core_for_channel(ch));
    REQUIRE(cores.size() == 3);
    REQUIRE(cores.count(0) == 1);
    REQUIRE(cores.count(1) == 1);
    REQUIRE(cores.count(2) == 1);
}

TEST_CASE("coreNum - RK3588 constant is 3", "[core]") {
    REQUIRE(RK3588 == 3);
}
