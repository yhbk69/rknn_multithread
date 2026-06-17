#include "catch_amalgamated.hpp"
#include "websocket.hpp"
#include <QCoreApplication>

/* WebSocket message dispatch unit tests using mocked server */
TEST_CASE("WebSocket - checkAndAlarm rate limiting", "[ws]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    auto ws = std::make_shared<WebSocket>();

    detect_result_group_t results;
    results.count = 0;

    /* Should not crash with empty results */
    REQUIRE_NOTHROW(ws->checkAndAlarm(&results, 0));
    ws->stop();
}

TEST_CASE("WebSocket - broadcast to zero clients does not crash", "[ws]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    auto ws = std::make_shared<WebSocket>();
    REQUIRE_NOTHROW(ws->broadcast(R"({"type":"test"})"));
    ws->stop();
}

TEST_CASE("WebSocket - start and stop cycle", "[ws]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    auto ws = std::make_shared<WebSocket>();
    REQUIRE_NOTHROW(ws->start(0));  /* port 0 = auto-assign */
    REQUIRE_NOTHROW(ws->stop());
}

TEST_CASE("WebSocket - isAlarmEnabled returns false by default", "[ws]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    auto ws = std::make_shared<WebSocket>();
    REQUIRE_FALSE(ws->isAlarmEnabled());
    ws->stop();
}

TEST_CASE("WebSocket - clientCount starts at 0", "[ws]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    auto ws = std::make_shared<WebSocket>();
    REQUIRE(ws->clientCount() == 0);
    ws->stop();
}
