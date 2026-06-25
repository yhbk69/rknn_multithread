#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <string>
#include <vector>
#include <libgen.h>
#include <cstdio>
#include <cstdlib>
#include <mutex>

#include "config.h"

/* 单个模型配置（级联场景） */
struct ModelConfig
{
    std::string name;
    std::string path;
    std::string type = "yolov5";
    int input_width = 640;
    int input_height = 640;
    int thread_num = 3;
    bool draw_result = true;
    std::string roi_from;
    std::vector<std::string> roi_class_names;
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

    std::vector<ModelConfig> models;
    bool is_cascade = false;

    AppConfig();
    void resolve_label_path();
};

AppConfig load_config(const char* config_path = DEFAULT_CONFIG_PATH, bool force_reload = false);
AppConfig reload_config(const char* config_path = DEFAULT_CONFIG_PATH);

#endif /* CONFIG_LOADER_H */
