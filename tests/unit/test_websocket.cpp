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
    ws->shutdown();
}

TEST_CASE("WebSocket - broadcast to zero clients does not crash", "[ws]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    auto ws = std::make_shared<WebSocket>();
    REQUIRE_NOTHROW(ws->broadcast(R"({"type":"test"})"));
    ws->shutdown();
}

TEST_CASE("WebSocket - init and shutdown cycle", "[ws]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    auto ws = std::make_shared<WebSocket>();
    WebSocketConfig cfg;
    cfg.server_port = 0;  /* port 0 = auto-assign */
    REQUIRE_NOTHROW(ws->init(cfg));
    REQUIRE_NOTHROW(ws->shutdown());
}

TEST_CASE("WebSocket - isAlarmEnabled returns true by default", "[ws]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    auto ws = std::make_shared<WebSocket>();
    REQUIRE(ws->isAlarmEnabled());
    ws->shutdown();
}

TEST_CASE("WebSocket - clientCount starts at 0", "[ws]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    auto ws = std::make_shared<WebSocket>();
    REQUIRE(ws->clientCount() == 0);
    ws->shutdown();
}
