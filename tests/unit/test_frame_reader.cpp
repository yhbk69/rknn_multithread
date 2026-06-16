#include "catch_amalgamated.hpp"
#include "pipeline/FrameReader.hpp"

TEST_CASE("FrameReader - invalid path open fails", "[reader]") {
    FrameReader reader("/nonexistent/video.mp4");
    REQUIRE_FALSE(reader.open());
    REQUIRE_FALSE(reader.isOpened());
}

TEST_CASE("FrameReader - read from unopened returns false", "[reader]") {
    FrameReader reader("/nonexistent/video.mp4");
    reader.open();
    cv::Mat frame;
    REQUIRE_FALSE(reader.read(frame));
}

TEST_CASE("FrameReader - close on unopened no crash", "[reader]") {
    FrameReader reader("/nonexistent/video.mp4");
    REQUIRE_NOTHROW(reader.close());
}

TEST_CASE("FrameReader - close after failed open no crash", "[reader]") {
    FrameReader reader("/nonexistent/video.mp4");
    reader.open();
    REQUIRE_NOTHROW(reader.close());
}

TEST_CASE("FrameReader - readWithRetry with invalid path", "[reader]") {
    FrameReader reader("/nonexistent/video.mp4", 10);
    cv::Mat frame;
    REQUIRE_FALSE(reader.readWithRetry(frame, 2));
}

TEST_CASE("FrameReader - double close no crash", "[reader]") {
    FrameReader reader("/nonexistent/video.mp4");
    reader.close();
    REQUIRE_NOTHROW(reader.close());
}

TEST_CASE("FrameReader - state after failed open", "[reader]") {
    FrameReader reader("/nonexistent/video.mp4");
    reader.open();
    REQUIRE_FALSE(reader.isOpened());
}
