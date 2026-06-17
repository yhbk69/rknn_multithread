#include "catch_amalgamated.hpp"
#include "pipeline/CascadePipeline.hpp"
#include "config_loader.hpp"

TEST_CASE("CascadePipeline - empty models init fails", "[cascade]") {
    CascadePipeline pipe;
    std::vector<ModelConfig> empty;
    int ret = pipe.init(empty, 0);
    REQUIRE(ret != 0);
    REQUIRE_FALSE(pipe.isCascade());
}

TEST_CASE("CascadePipeline - isCascade returns false for single model", "[cascade]") {
    CascadePipeline pipe;
    /* init with empty fails, so isCascade will be false by default */
    REQUIRE_FALSE(pipe.isCascade());
}

TEST_CASE("CascadePipeline - getLastDetectResult on empty pipeline returns empty", "[cascade]") {
    CascadePipeline pipe;
    auto res = pipe.getLastDetectResult();
    REQUIRE(res.count == 0);
}

TEST_CASE("CascadePipeline - set_thresholds on empty pipeline is safe", "[cascade]") {
    CascadePipeline pipe;
    REQUIRE_NOTHROW(pipe.set_thresholds(0.5f, 0.6f));
}

TEST_CASE("CascadePipeline - put on empty pipeline returns error", "[cascade]") {
    CascadePipeline pipe;
    cv::Mat dummy(100, 100, CV_8UC3);
    REQUIRE(pipe.put(dummy) != 0);
}

TEST_CASE("CascadePipeline - get on empty pipeline returns 1 (empty)", "[cascade]") {
    CascadePipeline pipe;
    cv::Mat out;
    REQUIRE(pipe.get(out) == 1);
}

/* ModelConfig struct construction tests */
TEST_CASE("CascadePipeline - ModelConfig defaults", "[cascade]") {
    ModelConfig mc;
    REQUIRE(mc.type == "yolov5");
    REQUIRE(mc.input_width == 640);
    REQUIRE(mc.input_height == 640);
    REQUIRE(mc.thread_num == 3);
    REQUIRE(mc.draw_result == true);
    REQUIRE(mc.roi_from.empty());
    REQUIRE(mc.roi_class_names.empty());
}

TEST_CASE("CascadePipeline - ModelConfig custom values", "[cascade]") {
    ModelConfig mc;
    mc.name = "test";
    mc.path = "test.rknn";
    mc.type = "yolo11";
    mc.input_width = 128;
    mc.input_height = 128;
    mc.thread_num = 2;
    mc.draw_result = false;
    mc.roi_from = "stage0";
    mc.roi_class_names = {"person"};
    REQUIRE(mc.name == "test");
    REQUIRE(mc.path == "test.rknn");
    REQUIRE(mc.type == "yolo11");
    REQUIRE(mc.input_width == 128);
    REQUIRE(mc.input_height == 128);
    REQUIRE(mc.thread_num == 2);
    REQUIRE(mc.draw_result == false);
    REQUIRE(mc.roi_from == "stage0");
    REQUIRE(mc.roi_class_names.size() == 1);
    REQUIRE(mc.roi_class_names[0] == "person");
}
