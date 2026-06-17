#include "catch_amalgamated.hpp"
#include "core/IEngine.hpp"
#include <opencv2/core.hpp>

/* Mock engine for interface contract testing */
class MockEngine : public IEngine {
    int w_ = 640, h_ = 640;
    float conf_ = 0.25f, nms_ = 0.45f;
    int init_called_ = 0;
    int detect_called_ = 0;
public:
    int init() override { init_called_++; return 0; }
    int detect(const cv::Mat &frame, detect_result_group_t *out) override {
        detect_called_++;
        if (out) {
            out->count = 0;
            out->id = 0;
        }
        return 0;
    }
    int getInputWidth() const override { return w_; }
    int getInputHeight() const override { return h_; }
    void setThresholds(float conf, float nms) override {
        conf_ = conf;
        nms_ = nms;
    }
    int initCount() const { return init_called_; }
    int detectCount() const { return detect_called_; }
    float conf() const { return conf_; }
    float nms() const { return nms_; }
};

TEST_CASE("IEngine - interface contract", "[engine]") {
    MockEngine eng;
    REQUIRE(eng.getInputWidth() == 640);
    REQUIRE(eng.getInputHeight() == 640);
    REQUIRE(eng.init() == 0);
    REQUIRE(eng.initCount() == 1);

    cv::Mat frame(480, 640, CV_8UC3);
    detect_result_group_t result;
    REQUIRE(eng.detect(frame, &result) == 0);
    REQUIRE(eng.detectCount() == 1);
}

TEST_CASE("IEngine - detect with empty frame does not crash", "[engine]") {
    MockEngine eng;
    eng.init();
    cv::Mat empty;
    detect_result_group_t result;
    eng.detect(empty, &result);  /* should not throw */
    REQUIRE(eng.detectCount() == 1);
}

TEST_CASE("IEngine - detect with null output pointer", "[engine]") {
    MockEngine eng;
    eng.init();
    cv::Mat frame(100, 100, CV_8UC3);
    eng.detect(frame, nullptr);  /* should not crash */
    REQUIRE(eng.detectCount() == 1);
}

TEST_CASE("IEngine - setThresholds updates internal state", "[engine]") {
    MockEngine eng;
    eng.setThresholds(0.5f, 0.6f);
    REQUIRE(eng.conf() == 0.5f);
    REQUIRE(eng.nms() == 0.6f);
}

TEST_CASE("IEngine - multiple init calls", "[engine]") {
    MockEngine eng;
    eng.init();
    eng.init();
    eng.init();
    REQUIRE(eng.initCount() == 3);
}

TEST_CASE("IEngine - dimensions are const after construction", "[engine]") {
    MockEngine eng;
    REQUIRE(eng.getInputWidth() == 640);
    REQUIRE(eng.getInputHeight() == 640);
}
