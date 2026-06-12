/*
 * config_loader.hpp - 运行时配置加载器
 * 
 * 从 config.json 加载运行时参数，提供默认值回退
 * 轻量级实现，不依赖第三方库
 */

#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <libgen.h>

#include "config.h"

/* 运行时配置结构体 */
struct AppConfig
{
    // 模型配置
    std::string model_path;
    std::string label_file;       // 仅文件名，路径从 model_path 推导
    int input_width;
    int input_height;

    // 检测参数
    float nms_threshold;
    float box_threshold;
    int class_num;

    // 运行时参数
    int thread_num;

    // Anchor 参数 (3组，每组6个值)
    int anchor_small[6];
    int anchor_medium[6];
    int anchor_large[6];

    // 标签文件完整路径（运行时推导）
    std::string label_path;

    AppConfig()
        : model_path("./model/RK3588/yolov5s-640-640.rknn"),
          label_file("coco_80_labels_list.txt"),
          input_width(640),
          input_height(640),
          nms_threshold(0.45f),
          box_threshold(0.25f),
          class_num(OBJ_CLASS_NUM),
          thread_num(3)
    {
        int default_small[] = {10, 13, 16, 30, 33, 23};
        int default_medium[] = {30, 61, 62, 45, 59, 119};
        int default_large[] = {116, 90, 156, 198, 373, 326};
        memcpy(anchor_small, default_small, sizeof(anchor_small));
        memcpy(anchor_medium, default_medium, sizeof(anchor_medium));
        memcpy(anchor_large, default_large, sizeof(anchor_large));
    }

    /* 从模型路径推导标签文件路径 */
    void resolve_label_path()
    {
        char* model_copy = strdup(model_path.c_str());
        char* dir = dirname(model_copy);
        label_path = std::string(dir) + "/../model/" + label_file;
        free(model_copy);
    }
};

/* 简单 JSON 解析辅助函数 */
static char* json_find_value(const char* json, const char* key)
{
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    const char* pos = strstr(json, search_key);
    if (!pos) return nullptr;

    pos = strchr(pos + strlen(search_key), ':');
    if (!pos) return nullptr;
    pos++;

    // 跳过空白
    while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') pos++;

    if (*pos == '"')
    {
        // 字符串值
        const char* start = pos + 1;
        const char* end = strchr(start, '"');
        if (!end) return nullptr;
        size_t len = end - start;
        char* val = (char*)malloc(len + 1);
        strncpy(val, start, len);
        val[len] = '\0';
        return val;
    }
    else
    {
        // 数值
        const char* start = pos;
        const char* end = start;
        while (*end && *end != ',' && *end != '}' && *end != ']' && *end != ' ' && *end != '\n') end++;
        size_t len = end - start;
        char* val = (char*)malloc(len + 1);
        strncpy(val, start, len);
        val[len] = '\0';
        return val;
    }
}

static std::vector<int> json_find_int_array(const char* json, const char* key)
{
    std::vector<int> result;
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    const char* pos = strstr(json, search_key);
    if (!pos) return result;

    pos = strchr(pos + strlen(search_key), ':');
    if (!pos) return result;
    pos = strchr(pos, '[');
    if (!pos) return result;
    pos++;

    while (*pos && *pos != ']')
    {
        if (*pos >= '0' && *pos <= '9')
        {
            result.push_back(atoi(pos));
            while (*pos >= '0' && *pos <= '9') pos++;
        }
        else
        {
            pos++;
        }
    }
    return result;
}

/* 加载配置文件 */
static AppConfig load_config(const char* config_path = DEFAULT_CONFIG_PATH)
{
    AppConfig config;

    FILE* fp = fopen(config_path, "r");
    if (!fp)
    {
        printf("Warning: Cannot open config file '%s', using defaults.\n", config_path);
        config.resolve_label_path();
        return config;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* json = (char*)malloc(size + 1);
    fread(json, 1, size, fp);
    json[size] = '\0';
    fclose(fp);

    // 解析 model 部分
    char* val;
    val = json_find_value(json, "path");
    if (val) { config.model_path = val; free(val); }

    val = json_find_value(json, "label_file");
    if (val) { config.label_file = val; free(val); }

    val = json_find_value(json, "input_width");
    if (val) { config.input_width = atoi(val); free(val); }

    val = json_find_value(json, "input_height");
    if (val) { config.input_height = atoi(val); free(val); }

    // 解析 detection 部分
    val = json_find_value(json, "nms_threshold");
    if (val) { config.nms_threshold = atof(val); free(val); }

    val = json_find_value(json, "box_threshold");
    if (val) { config.box_threshold = atof(val); free(val); }

    val = json_find_value(json, "class_num");
    if (val) { config.class_num = atoi(val); free(val); }

    // 解析 runtime 部分
    val = json_find_value(json, "thread_num");
    if (val) { config.thread_num = atoi(val); free(val); }

    // 解析 anchor 部分
    auto arr = json_find_int_array(json, "small");
    if (arr.size() == 6) memcpy(config.anchor_small, arr.data(), sizeof(config.anchor_small));

    arr = json_find_int_array(json, "medium");
    if (arr.size() == 6) memcpy(config.anchor_medium, arr.data(), sizeof(config.anchor_medium));

    arr = json_find_int_array(json, "large");
    if (arr.size() == 6) memcpy(config.anchor_large, arr.data(), sizeof(config.anchor_large));

    free(json);

    config.resolve_label_path();
    return config;
}

#endif /* CONFIG_LOADER_H */
