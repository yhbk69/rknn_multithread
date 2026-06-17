#include "catch_amalgamated.hpp"
#include "config_loader.hpp"
#include <cstdio>
#include <cstring>

static void write_json(const char *path, const char *content) {
    FILE *fp = fopen(path, "w");
    REQUIRE(fp != nullptr);
    fwrite(content, 1, strlen(content), fp);
    fclose(fp);
}

TEST_CASE("config_loader - valid config with all fields", "[cfg]") {
    write_json("/tmp/test_full.json", R"({
        "model": {
            "path": "./model/RK3588/yolov5s-640-640.rknn",
            "label_file": "coco_80_labels_list.txt",
            "input_width": 640,
            "input_height": 640
        },
        "detection": {
            "nms_threshold": 0.5,
            "box_threshold": 0.3,
            "class_num": 80
        },
        "runtime": { "thread_num": 4 },
        "anchor": {
            "small": [1,2,3,4,5,6],
            "medium": [7,8,9,10,11,12],
            "large": [13,14,15,16,17,18]
        },
        "websocket": {
            "host": "127.0.0.1",
            "port": 9003,
            "alarm_class_names": ["person", "car"]
        }
    })");
    AppConfig cfg = load_config("/tmp/test_full.json", true);
    REQUIRE(cfg.model_path == "./model/RK3588/yolov5s-640-640.rknn");
    REQUIRE(cfg.input_width == 640);
    REQUIRE(cfg.input_height == 640);
    REQUIRE(cfg.nms_threshold == 0.5f);
    REQUIRE(cfg.box_threshold == 0.3f);
    REQUIRE(cfg.class_num == 80);
    REQUIRE(cfg.thread_num == 4);
    REQUIRE(cfg.ws_host == "127.0.0.1");
    REQUIRE(cfg.ws_port == 9003);
    REQUIRE(cfg.alarm_class_names.size() == 2);
    REQUIRE(cfg.alarm_class_names[0] == "person");
    REQUIRE(cfg.alarm_class_names[1] == "car");
    REQUIRE(cfg.anchor_small[0] == 1);
    REQUIRE(cfg.anchor_small[5] == 6);
    std::remove("/tmp/test_full.json");
}

TEST_CASE("config_loader - missing file uses defaults", "[cfg]") {
    AppConfig cfg = load_config("/tmp/nonexistent_config.json", true);
    REQUIRE(cfg.model_path == "./model/RK3588/yolov5s-640-640.rknn");
    REQUIRE(cfg.input_width == 640);
    REQUIRE(cfg.nms_threshold == 0.45f);
    REQUIRE(cfg.box_threshold == 0.25f);
}

TEST_CASE("config_loader - minimal config uses defaults for missing fields", "[cfg]") {
    write_json("/tmp/test_minimal.json", R"({
        "model": { "path": "test.rknn" }
    })");
    AppConfig cfg = load_config("/tmp/test_minimal.json", true);
    REQUIRE(cfg.model_path == "test.rknn");
    REQUIRE(cfg.input_width == 640);
    REQUIRE(cfg.nms_threshold == 0.45f);
    REQUIRE(cfg.thread_num == 3);
    std::remove("/tmp/test_minimal.json");
}

TEST_CASE("config_loader - corrupted JSON falls back to defaults", "[cfg]") {
    write_json("/tmp/test_bad.json", "{ invalid json }");
    AppConfig cfg = load_config("/tmp/test_bad.json", true);
    REQUIRE(cfg.model_path == "./model/RK3588/yolov5s-640-640.rknn");
    std::remove("/tmp/test_bad.json");
}

TEST_CASE("config_loader - cascade models format", "[cfg]") {
    write_json("/tmp/test_cascade.json", R"({
        "models": [
            {
                "name": "detector",
                "path": "yolov5s.rknn",
                "type": "yolov5",
                "input_width": 640,
                "input_height": 640,
                "thread_num": 3,
                "draw_result": true
            },
            {
                "name": "classifier",
                "path": "yolo11n-face.rknn",
                "type": "yolo11",
                "input_width": 128,
                "input_height": 128,
                "thread_num": 2,
                "draw_result": false,
                "roi_from": "detector",
                "roi_class_names": ["person"]
            }
        ]
    })");
    AppConfig cfg = load_config("/tmp/test_cascade.json", true);
    REQUIRE(cfg.is_cascade == true);
    REQUIRE(cfg.models.size() == 2);
    REQUIRE(cfg.models[0].name == "detector");
    REQUIRE(cfg.models[0].type == "yolov5");
    REQUIRE(cfg.models[0].thread_num == 3);
    REQUIRE(cfg.models[0].draw_result == true);
    REQUIRE(cfg.models[1].name == "classifier");
    REQUIRE(cfg.models[1].type == "yolo11");
    REQUIRE(cfg.models[1].input_width == 128);
    REQUIRE(cfg.models[1].input_height == 128);
    REQUIRE(cfg.models[1].thread_num == 2);
    REQUIRE(cfg.models[1].draw_result == false);
    REQUIRE(cfg.models[1].roi_from == "detector");
    REQUIRE(cfg.models[1].roi_class_names.size() == 1);
    REQUIRE(cfg.models[1].roi_class_names[0] == "person");
    std::remove("/tmp/test_cascade.json");
}

TEST_CASE("config_loader - cascade model path fallback to first model", "[cfg]") {
    write_json("/tmp/test_cascade2.json", R"({
        "models": [
            { "path": "first.rknn", "type": "yolov5", "input_width": 320, "input_height": 320 },
            { "path": "second.rknn", "type": "yolo11", "input_width": 128, "input_height": 128 }
        ]
    })");
    AppConfig cfg = load_config("/tmp/test_cascade2.json", true);
    REQUIRE(cfg.model_path == "first.rknn");
    REQUIRE(cfg.input_width == 320);
    REQUIRE(cfg.input_height == 320);
    std::remove("/tmp/test_cascade2.json");
}

TEST_CASE("config_loader - websocket alarm class names", "[cfg]") {
    write_json("/tmp/test_ws.json", R"({
        "model": { "path": "m.rknn" },
        "websocket": {
            "alarm_class_names": ["person", "bicycle", "car"]
        }
    })");
    AppConfig cfg = load_config("/tmp/test_ws.json", true);
    REQUIRE(cfg.alarm_class_names.size() == 3);
    REQUIRE(cfg.alarm_class_names[0] == "person");
    REQUIRE(cfg.alarm_class_names[2] == "car");
    std::remove("/tmp/test_ws.json");
}
