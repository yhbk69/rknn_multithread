/*
 * config_loader.hpp - 运行时配置加载器
 * 
 * 从 config.json 加载运行时参数，提供默认值回退
 * 使用 nlohmann/json 解析
 */

#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <string>
#include <vector>
#include <libgen.h>
#include <cstdio>
#include <cstdlib>

#include "nlohmann/json.hpp"
#include "config.h"

using json = nlohmann::json;

/* 单个模型配置（级联场景） */
struct ModelConfig
{
    std::string name;
    std::string path;
    std::string type = "yolov5";     // "yolov5" | "yolo11"
    int input_width = 640;
    int input_height = 640;
    int thread_num = 3;
    bool draw_result = true;
    std::string roi_from;             // 上一 stage 名称（空表示不使用 ROI）
    std::vector<std::string> roi_class_names; // 只对这些类名裁剪 ROI（如 ["person"]）
};

/* 运行时配置结构体 */
struct AppConfig
{
    std::string model_path;
    std::string label_file;
    int input_width;
    int input_height;

    float nms_threshold;
    float box_threshold;
    int class_num;

    int thread_num;

    int anchor_small[6];
    int anchor_medium[6];
    int anchor_large[6];

    std::string ws_host;
    int ws_port;
    std::vector<std::string> alarm_class_names;
    std::string alarm_screenshot_dir;

    std::string label_path;

    std::vector<ModelConfig> models;   // 多模型级联配置
    bool is_cascade = false;          // 是否使用级联模式

    AppConfig()
        : model_path("./model/RK3588/yolov5s-640-640.rknn"),
          label_file("coco_80_labels_list.txt"),
          input_width(640),
          input_height(640),
          nms_threshold(0.45f),
          box_threshold(0.25f),
          class_num(OBJ_CLASS_NUM),
          thread_num(3),
          ws_host("0.0.0.0"),
          ws_port(9002)
    {
        int default_small[] = {10, 13, 16, 30, 33, 23};
        int default_medium[] = {30, 61, 62, 45, 59, 119};
        int default_large[] = {116, 90, 156, 198, 373, 326};
        memcpy(anchor_small, default_small, sizeof(anchor_small));
        memcpy(anchor_medium, default_medium, sizeof(anchor_medium));
        memcpy(anchor_large, default_large, sizeof(anchor_large));
    }

    void resolve_label_path()
    {
        char* model_copy = strdup(model_path.c_str());
        char* dir = dirname(model_copy);
        label_path = std::string(dir) + "/../model/" + label_file;
        free(model_copy);
    }
};

/* 加载配置文件（带缓存，多次调用只解析一次）
 * force_reload = true 时强制重新解析（用于热更新场景） */
inline AppConfig load_config(const char* config_path = DEFAULT_CONFIG_PATH, bool force_reload = false)
{
    static AppConfig cached;
    static bool loaded = false;
    if (loaded && !force_reload)
        return cached;

    AppConfig config;

    FILE* fp = fopen(config_path, "r");
    if (!fp)
    {
        printf("Warning: Cannot open config file '%s', using defaults.\n", config_path);
        config.resolve_label_path();
        if (!loaded) { cached = config; loaded = true; }
        return config;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* buf = (char*)malloc(size + 1);
    fread(buf, 1, size, fp);
    buf[size] = '\0';
    fclose(fp);

    json j;
    try {
        j = json::parse(buf);
    } catch (const json::parse_error& e) {
        printf("Warning: JSON parse error: %s, using defaults.\n", e.what());
        free(buf);
        config.resolve_label_path();
        if (!loaded) { cached = config; loaded = true; }
        return config;
    }
    free(buf);

    if (j.contains("models") && j["models"].is_array() && j["models"].size() > 0) {
        /* 新级联格式：多模型数组 */
        config.is_cascade = true;
        for (const auto& m : j["models"]) {
            ModelConfig mc;
            if (m.contains("name"))        mc.name = m["name"].get<std::string>();
            if (m.contains("path"))        mc.path = m["path"].get<std::string>();
            if (m.contains("type"))        mc.type = m["type"].get<std::string>();
            if (m.contains("input_width")) mc.input_width = m["input_width"].get<int>();
            if (m.contains("input_height"))mc.input_height = m["input_height"].get<int>();
            if (m.contains("thread_num"))  mc.thread_num = m["thread_num"].get<int>();
            if (m.contains("draw_result")) mc.draw_result = m["draw_result"].get<bool>();
            if (m.contains("roi_from"))    mc.roi_from = m["roi_from"].get<std::string>();
            if (m.contains("roi_class_names") && m["roi_class_names"].is_array()) {
                for (const auto& c : m["roi_class_names"])
                    mc.roi_class_names.push_back(c.get<std::string>());
            }
            config.models.push_back(mc);
        }
        /* 级联模式下，使用第一个模型作为主模型参数（兼容旧代码） */
        config.model_path  = config.models[0].path;
        config.input_width = config.models[0].input_width;
        config.input_height = config.models[0].input_height;
        config.thread_num  = config.models[0].thread_num;
    } else if (j.contains("model")) {
        /* 旧格式：单模型 */
        const auto& m = j["model"];
        if (m.contains("path"))        config.model_path = m["path"].get<std::string>();
        if (m.contains("label_file"))  config.label_file = m["label_file"].get<std::string>();
        if (m.contains("input_width")) config.input_width = m["input_width"].get<int>();
        if (m.contains("input_height"))config.input_height = m["input_height"].get<int>();
    }

    if (j.contains("detection")) {
        const auto& d = j["detection"];
        if (d.contains("nms_threshold"))  config.nms_threshold = d["nms_threshold"].get<float>();
        if (d.contains("box_threshold"))  config.box_threshold = d["box_threshold"].get<float>();
        if (d.contains("class_num"))      config.class_num = d["class_num"].get<int>();
    }

    if (j.contains("runtime")) {
        const auto& r = j["runtime"];
        if (r.contains("thread_num")) config.thread_num = r["thread_num"].get<int>();
    }

    if (j.contains("anchor")) {
        const auto& a = j["anchor"];
        auto fill_arr = [](const json& arr, int* out) {
            if (arr.is_array() && arr.size() == 6)
                for (int i = 0; i < 6; i++) out[i] = arr[i].get<int>();
        };
        if (a.contains("small"))  fill_arr(a["small"], config.anchor_small);
        if (a.contains("medium")) fill_arr(a["medium"], config.anchor_medium);
        if (a.contains("large"))  fill_arr(a["large"], config.anchor_large);
    }

    if (j.contains("websocket")) {
        const auto& w = j["websocket"];
        if (w.contains("host")) config.ws_host = w["host"].get<std::string>();
        if (w.contains("port")) config.ws_port = w["port"].get<int>();
        if (w.contains("alarm_class_names") && w["alarm_class_names"].is_array()) {
            config.alarm_class_names.clear();
            for (const auto& name : w["alarm_class_names"])
                config.alarm_class_names.push_back(name.get<std::string>());
        }
        if (w.contains("alarm_screenshot_dir"))
            config.alarm_screenshot_dir = w["alarm_screenshot_dir"].get<std::string>();
    }

    config.resolve_label_path();
    cached = config;
    loaded = true;
    return config;
}

/* 强制重新加载配置（热更新时使用） */
inline AppConfig reload_config(const char* config_path = DEFAULT_CONFIG_PATH)
{
    return load_config(config_path, true);
}

#endif /* CONFIG_LOADER_H */
