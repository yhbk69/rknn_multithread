#include "catch_amalgamated.hpp"
#include "pipeline/ResultRenderer.hpp"

TEST_CASE("ResultRenderer - toQImage 640x480", "[renderer]") {
    ResultRenderer r;
    cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(128, 128, 128));
    QImage img = r.toQImage(frame);
    REQUIRE(img.width() == 640);
    REQUIRE(img.height() == 480);
    REQUIRE(img.format() == QImage::Format_RGB888);
    REQUIRE_FALSE(img.isNull());
}

TEST_CASE("ResultRenderer - toQImage empty mat", "[renderer]") {
    ResultRenderer r;
    cv::Mat empty;
    QImage img = r.toQImage(empty);
    REQUIRE(img.isNull());
}

TEST_CASE("ResultRenderer - toQImage 1x1", "[renderer]") {
    ResultRenderer r;
    cv::Mat frame(1, 1, CV_8UC3, cv::Scalar(255, 0, 0));
    QImage img = r.toQImage(frame);
    REQUIRE(img.width() == 1);
    REQUIRE(img.height() == 1);
    REQUIRE_FALSE(img.isNull());
}

TEST_CASE("ResultRenderer - drawFps no crash", "[renderer]") {
    ResultRenderer r;
    cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(0, 0, 0));
    REQUIRE_NOTHROW(r.drawFps(frame, 30.5));
    REQUIRE_NOTHROW(r.drawFps(frame, 0.0));
    REQUIRE_NOTHROW(r.drawFps(frame, -1.0));
    REQUIRE_NOTHROW(r.drawFps(frame, 999.99));
}

TEST_CASE("ResultRenderer - drawFps then toQImage", "[renderer]") {
    ResultRenderer r;
    cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(0, 0, 0));
    r.drawFps(frame, 30.0);
    QImage img = r.toQImage(frame);
    REQUIRE(img.width() == 640);
    REQUIRE(img.height() == 480);
    REQUIRE_FALSE(img.isNull());
}

TEST_CASE("ResultRenderer - multiple toQImage calls", "[renderer]") {
    ResultRenderer r;
    cv::Mat frame(100, 100, CV_8UC3, cv::Scalar(64, 128, 192));
    QImage img1 = r.toQImage(frame);
    QImage img2 = r.toQImage(frame);
    REQUIRE(img1.size() == img2.size());
}

TEST_CASE("ResultRenderer - toQImage preserves dimensions", "[renderer]") {
    ResultRenderer r;
    cv::Mat frame(200, 300, CV_8UC3, cv::Scalar(0, 0, 0));
    QImage img = r.toQImage(frame);
    REQUIRE(img.width() == 300);
    REQUIRE(img.height() == 200);
}
