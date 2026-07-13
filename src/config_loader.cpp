/*
 * config_loader.cpp - 配置加载模块实现
 *
 * 从 config.json 加载运行时配置，支持：
 *   - 单模型模式（"model" 节）
 *   - 级联多模型模式（"models" 数组）
 *   - 线程安全的配置缓存（std::once_flag）
 *   - 热重载（force_reload）
 *
 * 配置文件格式（config.json）：
 *   {
 *     "model": { "path": "...", "label_file": "...", ... },
 *     "detection": { "box_threshold": 0.25, ... },
 *     "anchor": { "small": [...], "medium": [...], "large": [...] },
 *     "runtime": { "thread_num": 3 },
 *     "websocket": { "host": "0.0.0.0", "port": 9002, ... }
 *   }
 */

#include "config_loader.hpp"

#include <libgen.h>
#include <cstring>
#include <mutex>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

/**
 * AppConfig 构造函数：加载默认配置值
 */
AppConfig::AppConfig()
    : model_path("./model/RK3588/yolov5s-640-640.rknn"),
      label_file("coco_80_labels_list.txt"),
      input_width(640),
      input_height(640),
      nms_threshold(0.45f),
      box_threshold(0.25f),
      class_num(OBJ_CLASS_NUM),  // 默认 80 类（COCO）
      thread_num(3),
      ws_host("0.0.0.0"),
      ws_port(9002)
{
    // YOLOv5 默认 anchor（COCO 数据集）
    int default_small[] = {10, 13, 16, 30, 33, 23};
    int default_medium[] = {30, 61, 62, 45, 59, 119};
    int default_large[] = {116, 90, 156, 198, 373, 326};
    memcpy(anchor_small, default_small, sizeof(anchor_small));
    memcpy(anchor_medium, default_medium, sizeof(anchor_medium));
    memcpy(anchor_large, default_large, sizeof(anchor_large));
}

/**
 * 根据模型路径解析标签文件路径
 *
 * 路径规则：模型在 model/RK3588/xxx.rknn，标签在 model/xxx.txt
 * 例如：./model/RK3588/yolov5s.rknn → ./model/coco_80_labels_list.txt
 */
void AppConfig::resolve_label_path()
{
    char* model_copy = strdup(model_path.c_str());
    char* dir = dirname(model_copy);
    label_path = std::string(dir) + "/../model/" + label_file;
    free(model_copy);
}

/**
 * 从 JSON 解析配置到 AppConfig 结构体
 *
 * 支持的配置节：
 *   - "models": 级联多模型配置
 *   - "model": 单模型配置
 *   - "detection": 检测阈值
 *   - "runtime": 运行时参数
 *   - "anchor": 锚框参数
 *   - "websocket": WebSocket 配置
 *
 * @param j      解析后的 JSON 对象
 * @param config 输出配置结构体
 */
static void parseConfigFromJson(const json& j, AppConfig& config) {
    // ========== 级联多模型配置 ==========
    if (j.contains("models") && j["models"].is_array() && j["models"].size() > 0) {
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
        // 使用第一个模型的配置作为全局配置
        config.model_path  = config.models[0].path;
        config.input_width = config.models[0].input_width;
        config.input_height = config.models[0].input_height;
        config.thread_num  = config.models[0].thread_num;
    } else if (j.contains("model")) {
        // ========== 单模型配置 ==========
        const auto& m = j["model"];
        if (m.contains("path"))        config.model_path = m["path"].get<std::string>();
        if (m.contains("label_file"))  config.label_file = m["label_file"].get<std::string>();
        if (m.contains("input_width")) config.input_width = m["input_width"].get<int>();
        if (m.contains("input_height"))config.input_height = m["input_height"].get<int>();
    }

    // ========== 检测阈值 ==========
    if (j.contains("detection")) {
        const auto& d = j["detection"];
        if (d.contains("nms_threshold"))  config.nms_threshold = d["nms_threshold"].get<float>();
        if (d.contains("box_threshold"))  config.box_threshold = d["box_threshold"].get<float>();
        if (d.contains("class_num"))      config.class_num = d["class_num"].get<int>();
    }

    // ========== 运行时参数 ==========
    if (j.contains("runtime")) {
        const auto& r = j["runtime"];
        if (r.contains("thread_num")) config.thread_num = r["thread_num"].get<int>();
    }

    // ========== Anchor 参数 ==========
    if (j.contains("anchor")) {
        const auto& a = j["anchor"];
        // 辅助 lambda：从 JSON 数组填充 int[6]
        auto fill_arr = [](const json& arr, int* out) {
            if (arr.is_array() && arr.size() == 6)
                for (int i = 0; i < 6; i++) out[i] = arr[i].get<int>();
        };
        if (a.contains("small"))  fill_arr(a["small"], config.anchor_small);
        if (a.contains("medium")) fill_arr(a["medium"], config.anchor_medium);
        if (a.contains("large"))  fill_arr(a["large"], config.anchor_large);
    }

    // ========== WebSocket 配置 ==========
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
}

/**
 * 加载配置文件并解析
 *
 * @param config_path 配置文件路径
 * @return 解析后的 AppConfig 结构体
 */
static AppConfig loadConfigFile(const char* config_path) {
    AppConfig config;

    // 打开配置文件
    FILE* fp = fopen(config_path, "r");
    if (!fp) {
        printf("Warning: Cannot open config file '%s', using defaults.\n", config_path);
        config.resolve_label_path();
        return config;
    }

    // 读取整个文件
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* buf = (char*)malloc(size + 1);
    fread(buf, 1, size, fp);
    buf[size] = '\0';
    fclose(fp);

    // 解析 JSON
    json j;
    try {
        j = json::parse(buf);
    } catch (const json::parse_error& e) {
        printf("Warning: JSON parse error: %s, using defaults.\n", e.what());
        free(buf);
        config.resolve_label_path();
        return config;
    }
    free(buf);

    // 从 JSON 提取配置
    parseConfigFromJson(j, config);
    config.resolve_label_path();
    return config;
}

/**
 * 加载配置（线程安全，首次加载后缓存）
 *
 * 使用 std::once_flag 保证多线程环境下只加载一次。
 * force_reload=true 时强制重新加载（用于热重载）。
 *
 * @param config_path  配置文件路径
 * @param force_reload 是否强制重新加载
 * @return AppConfig 配置结构体
 */
AppConfig load_config(const char* config_path, bool force_reload)
{
    static AppConfig cached;          // 缓存的配置
    static std::once_flag loaded_flag; // 一次性加载标志
    static bool loaded = false;        // 是否已加载
    static std::mutex reload_mtx;      // 热重载互斥锁

    // 强制重新加载（热重载场景）
    if (force_reload) {
        std::lock_guard<std::mutex> lock(reload_mtx);
        cached = loadConfigFile(config_path);
        loaded = true;
        return cached;
    }

    // 首次加载（线程安全）
    std::call_once(loaded_flag, [&]() {
        cached = loadConfigFile(config_path);
        loaded = true;
    });

    return cached;
}

/**
 * 强制重新加载配置（热重载）
 * @param config_path 配置文件路径
 * @return AppConfig 配置结构体
 */
AppConfig reload_config(const char* config_path)
{
    return load_config(config_path, true);
}
