/**
 * @file config.hpp
 * @brief 统一 YOLO 检测平台 — 编译期常量配置
 *
 * 所有编译期固定的配置参数集中管理。
 * 运行时可调参数以 RuntimeConfig 为准，
 * 此处的默认值需与 RuntimeConfig 保持同步。
 */

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <opencv2/core.hpp>

namespace Config {

    // ---- 模型默认配置 ----
    constexpr int INPUT_WIDTH   = 640;
    constexpr int INPUT_HEIGHT  = 640;

    // ---- 检测默认阈值 ----
    constexpr float CONF_THRESHOLD   = 0.25f;
    constexpr float IOU_THRESHOLD    = 0.45f;

    // ---- COCO 80 类标签（默认，可通过 config.json 覆盖） ----
    constexpr int NUM_CLASSES = 80;

    // Letterbox 填充色（BGR，与 YOLO 训练一致）
    const cv::Scalar LETTERBOX_FILL_COLOR = cv::Scalar(114, 114, 114);

    // ---- 平台检测 ----
#if defined(_WIN32) || defined(_WIN64)
    constexpr bool IS_WINDOWS = true;
#elif defined(__linux__) && defined(__aarch64__)
    constexpr bool IS_RK3588 = true;
#else
    constexpr bool IS_WINDOWS = false;
    constexpr bool IS_RK3588  = false;
#endif

    // ---- 网络默认端口 ----
    constexpr int WEBSOCKET_PORT = 9090;    // WS 控制通道
    constexpr int ALERT_WS_PORT  = 9091;    // 报警推送
    constexpr int STREAM_PORT    = 9092;    // 视频流
    constexpr int HTTP_PORT      = 9093;    // HTTP 文件服务

    // ---- 报警默认参数 ----
    constexpr int ACK_TIMEOUT_MS      = 5000;   // ACK 超时
    constexpr int RING_BUFFER_FRAMES  = 90;     // 环形缓冲区帧数（~3s@30fps）
    constexpr int ALERT_AFTER_FRAMES  = 60;     // 报警触发后再录帧数
    constexpr int ALERT_COOLDOWN_MS   = 10000;  // 同类别报警冷却

    // ---- 录像默认参数 ----
    constexpr int    RECORD_KEEP_DAYS = 7;
    constexpr double RECORD_MAX_SIZE_GB = 10.0;

    // ---- 路径默认值 ----
    const std::string DEFAULT_ONNX_MODEL = "model/yolo11n.onnx";
    const std::string DEFAULT_RKNN_MODEL = "model/RK3588/yolov5s-640-640.rknn";
    const std::string DEFAULT_LABEL_FILE = "model/coco_80_labels_list.txt";
    const std::string DEFAULT_OUTPUT_DIR = "output";
    const std::string DEFAULT_RECORD_DIR = "recordings";

} // namespace Config

#endif // CONFIG_HPP
