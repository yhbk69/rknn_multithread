#include "catch_amalgamated.hpp"
#include "preprocess.h"

static float min_scale(const cv::Mat &img, const cv::Size &target) {
    return std::min((float)target.width / img.cols, (float)target.height / img.rows);
}

TEST_CASE("letterbox - exact match size", "[pre]") {
    cv::Mat img(640, 640, CV_8UC3, cv::Scalar(100, 150, 200));
    cv::Mat out;
    BOX_RECT pads;
    cv::Size target(640, 640);
    float s = min_scale(img, target);
    letterbox(img, out, pads, s, target);

    REQUIRE(out.cols == 640);
    REQUIRE(out.rows == 640);
    REQUIRE(pads.left == 0);
    REQUIRE(pads.right == 0);
    REQUIRE(pads.top == 0);
    REQUIRE(pads.bottom == 0);
}

TEST_CASE("letterbox - wider image gets horizontal pads", "[pre]") {
    cv::Mat img(480, 640, CV_8UC3, cv::Scalar(100, 150, 200));
    cv::Mat out;
    BOX_RECT pads;
    cv::Size target(640, 640);
    float s = min_scale(img, target);
    letterbox(img, out, pads, s, target);

    REQUIRE(out.cols == 640);
    REQUIRE(out.rows == 640);
    /* scale=1.0, new_w=640, new_h=480 → pad_top+pad_bottom=160 */
    REQUIRE(pads.left == 0);
    REQUIRE(pads.right == 0);
    REQUIRE(pads.top + pads.bottom == 160);
}

TEST_CASE("letterbox - taller image gets vertical pads", "[pre]") {
    cv::Mat img(640, 480, CV_8UC3, cv::Scalar(100, 150, 200));
    cv::Mat out;
    BOX_RECT pads;
    cv::Size target(640, 640);
    float s = min_scale(img, target);
    letterbox(img, out, pads, s, target);

    REQUIRE(out.cols == 640);
    REQUIRE(out.rows == 640);
    /* scale=1.0, new_w=480, new_h=640 → pad_left+pad_right=160 */
    REQUIRE(pads.top == 0);
    REQUIRE(pads.bottom == 0);
    REQUIRE(pads.left + pads.right == 160);
}

TEST_CASE("letterbox - generic aspect ratio", "[pre]") {
    cv::Mat img(360, 480, CV_8UC3);
    cv::Mat out;
    BOX_RECT pads;
    cv::Size target(640, 640);
    float s = min_scale(img, target);
    letterbox(img, out, pads, s, target);

    REQUIRE(out.cols == 640);
    REQUIRE(out.rows == 640);
    int new_w = (int)(img.cols * s);
    int new_h = (int)(img.rows * s);
    REQUIRE(pads.left + pads.right == target.width - new_w);
    REQUIRE(pads.top + pads.bottom == target.height - new_h);
}

TEST_CASE("letterbox - output image has correct pixel type", "[pre]") {
    cv::Mat img(100, 200, CV_8UC3);
    cv::Mat out;
    BOX_RECT pads;
    cv::Size target(640, 640);
    float s = min_scale(img, target);
    letterbox(img, out, pads, s, target);

    REQUIRE(out.type() == CV_8UC3);
}

TEST_CASE("letterbox - non-square target size", "[pre]") {
    /* 1280x720 -> 640x384 */
    cv::Mat img(720, 1280, CV_8UC3);
    cv::Mat out;
    BOX_RECT pads;
    cv::Size target(640, 384);
    float s = min_scale(img, target);
    letterbox(img, out, pads, s, target);

    REQUIRE(out.cols == 640);
    REQUIRE(out.rows == 384);
    int new_w = (int)(img.cols * s);
    int new_h = (int)(img.rows * s);
    REQUIRE(pads.left + pads.right == target.width - new_w);
    REQUIRE(pads.top + pads.bottom == target.height - new_h);
}
