#include "catch_amalgamated.hpp"
#include "postprocess.h"
#include "config.h"
#include <vector>

static constexpr int MODEL_SIZE = 640;

static std::vector<int8_t> make_scale_data(int grid_h, int grid_w, int8_t value = 0) {
    return std::vector<int8_t>(PROP_BOX_SIZE * 3 * grid_h * grid_w, value);
}

TEST_CASE("PostProcessContext - init with fixtures", "[post]") {
    PostProcessContext ctx;
    int ret = ctx.init("tests/fixtures/model/test.rknn");
    REQUIRE(ret == 0);
    REQUIRE(ctx.isInitialized());
    ctx.deinit();
    REQUIRE_FALSE(ctx.isInitialized());
}

TEST_CASE("PostProcessContext - double init", "[post]") {
    PostProcessContext ctx;
    REQUIRE(ctx.init("tests/fixtures/model/test.rknn") == 0);
    REQUIRE(ctx.init("tests/fixtures/model/test.rknn") == 0);
    ctx.deinit();
}

TEST_CASE("PostProcessContext - init nonexistent path", "[post]") {
    PostProcessContext ctx;
    REQUIRE(ctx.init("/nonexistent/path/model.rknn") != 0);
    REQUIRE_FALSE(ctx.isInitialized());
}

TEST_CASE("PostProcessContext - deinit without init", "[post]") {
    PostProcessContext ctx;
    REQUIRE_NOTHROW(ctx.deinit());
}

TEST_CASE("PostProcessContext - zero input gives zero detections", "[post]") {
    PostProcessContext ctx;
    REQUIRE(ctx.init("tests/fixtures/model/test.rknn") == 0);

    int grid_8 = MODEL_SIZE / 8;
    int grid_16 = MODEL_SIZE / 16;
    int grid_32 = MODEL_SIZE / 32;

    auto in0 = make_scale_data(grid_8, grid_8);
    auto in1 = make_scale_data(grid_16, grid_16);
    auto in2 = make_scale_data(grid_32, grid_32);

    std::vector<int32_t> qnt_zps = {128, 128, 128};
    std::vector<float> qnt_scales = {0.02f, 0.02f, 0.02f};
    BOX_RECT pads = {0, 0, 0, 0};
    detect_result_group_t result;

    ctx.process(in0.data(), in1.data(), in2.data(),
                MODEL_SIZE, MODEL_SIZE, 0.25f, 0.45f,
                pads, 1.0f, 1.0f,
                qnt_zps, qnt_scales, &result);

    REQUIRE(result.count == 0);
    ctx.deinit();
}

TEST_CASE("PostProcessContext - one known detection", "[post]") {
    PostProcessContext ctx;
    REQUIRE(ctx.init("tests/fixtures/model/test.rknn") == 0);

    int stride = 8;
    int grid_h = MODEL_SIZE / stride;
    int grid_w = MODEL_SIZE / stride;
    int grid_len = grid_h * grid_w;

    auto in0 = make_scale_data(grid_h, grid_w);
    auto in1 = make_scale_data(MODEL_SIZE / 16, MODEL_SIZE / 16);
    auto in2 = make_scale_data(MODEL_SIZE / 32, MODEL_SIZE / 32);

    // Use zp=0, scale=1.0 so simple int8_t values map directly to float.
    // box_confidence=1 > 0.25 threshold, class prob=1 > 0.25 threshold.
    int8_t high = 1;
    int8_t zero = 0;
    int anchor_a = 0;
    int cell_i = 0, cell_j = 0;

    // box_confidence: input[(85*a+4)*grid_len + i*gw + j]
    in0[(PROP_BOX_SIZE * anchor_a + 4) * grid_len + cell_i * grid_w + cell_j] = high;
    // class 0 probability: input[(85*a+5)*grid_len + i*gw + j]
    in0[(PROP_BOX_SIZE * anchor_a + 5) * grid_len + cell_i * grid_w + cell_j] = high;
    // box_xywh at offset anchor_a * (85*grid_len) + i*gw + j, with per-component stride of grid_len
    in0[(PROP_BOX_SIZE * anchor_a) * grid_len + cell_i * grid_w + cell_j] = zero;
    in0[(PROP_BOX_SIZE * anchor_a) * grid_len + cell_i * grid_w + cell_j + grid_len] = zero;
    in0[(PROP_BOX_SIZE * anchor_a) * grid_len + cell_i * grid_w + cell_j + 2 * grid_len] = zero;
    in0[(PROP_BOX_SIZE * anchor_a) * grid_len + cell_i * grid_w + cell_j + 3 * grid_len] = zero;

    std::vector<int32_t> qnt_zps = {0, 0, 0};
    std::vector<float> qnt_scales = {1.0f, 1.0f, 1.0f};
    BOX_RECT pads = {0, 0, 0, 0};
    detect_result_group_t result;

    int rc = ctx.process(in0.data(), in1.data(), in2.data(),
                MODEL_SIZE, MODEL_SIZE, 0.25f, 0.45f,
                pads, 1.0f, 1.0f,
                qnt_zps, qnt_scales, &result);

    REQUIRE(rc == 0);
    REQUIRE(result.count == 1);
    REQUIRE(result.results[0].prop == 1.0f);
    REQUIRE(result.results[0].box.left == 0);
    ctx.deinit();
}
