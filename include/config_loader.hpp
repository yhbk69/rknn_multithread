/*
 * config_loader.hpp - 配置加载模块
 *
 * 从 config.json 加载运行时配置，支持：
 *   - 单模型模式（"model" 节）
 *   - 级联多模型模式（"models" 数组）
 *   - 线程安全的配置缓存和热重载
 *
 * 使用示例：
 *   AppConfig cfg = load_config();           // 首次加载（自动缓存）
 *   AppConfig cfg = reload_config();         // 强制重新加载
 *   cfg.resolve_label_path();                // 解析标签文件路径
 */

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
    std::string name;                    // 模型名称（用于日志和 WebSocket 查询）
    std::string path;                    // RKNN 模型文件路径
    std::string type = "yolov5";         // 模型类型（"yolov5" 或 "yolo11"）
    int input_width = 640;               // 模型输入宽度
    int input_height = 640;              // 模型输入高度
    int thread_num = 3;                  // NPU 推理线程数
    bool draw_result = true;             // 是否在帧上绘制检测框
    std::string roi_from;                // ROI 来源 stage 名称（级联场景）
    std::vector<std::string> roi_class_names;  // ROI 过滤的类别名列表
};

/* 运行时配置结构体 */
struct AppConfig
{
    /* 模型配置 */
    std::string model_path;              // RKNN 模型文件路径
    std::string label_file;              // 标签文件名（如 "coco_80_labels_list.txt"）
    int input_width;                     // 模型输入宽度
    int input_height;                    // 模型输入高度

    /* 检测阈值 */
    float nms_threshold;                 // NMS IoU 阈值
    float box_threshold;                 // 置信度阈值
    int class_num;                       // 类别数（COCO=80）

    /* 推理配置 */
    int thread_num;                      // NPU 推理线程数

    /* Anchor 参数（YOLOv5 使用） */
    int anchor_small[6];                 // 小尺度 anchor（3组×2）
    int anchor_medium[6];                // 中尺度 anchor
    int anchor_large[6];                 // 大尺度 anchor

    /* WebSocket 配置 */
    std::string ws_host;                 // WebSocket 监听地址
    int ws_port;                         // WebSocket 监听端口
    std::vector<std::string> alarm_class_names;  // 需要报警的类别名列表
    std::string alarm_screenshot_dir;    // 报警截图保存目录

    /* 标签路径（运行时解析） */
    std::string label_path;              // 标签文件完整路径

    /* 级联模型配置 */
    std::vector<ModelConfig> models;     // 模型配置列表（级联场景）
    bool is_cascade = false;             // 是否为级联模式

    AppConfig();                         // 构造：加载默认值
    void resolve_label_path();           // 根据模型路径解析标签文件路径
};

/*
 * 加载配置（线程安全，首次加载后缓存）
 * @param config_path  配置文件路径（默认 "config.json"）
 * @param force_reload 是否强制重新加载
 * @return AppConfig 配置结构体
 */
AppConfig load_config(const char* config_path = DEFAULT_CONFIG_PATH, bool force_reload = false);

/*
 * 强制重新加载配置（热重载）
 * @param config_path 配置文件路径
 * @return AppConfig 配置结构体
 */
AppConfig reload_config(const char* config_path = DEFAULT_CONFIG_PATH);

#endif /* CONFIG_LOADER_H */
