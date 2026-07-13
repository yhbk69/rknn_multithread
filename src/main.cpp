/*
 * main.cpp - CLI 模式入口（无 Qt GUI）
 *
 * 使用流程：
 *   1. 加载配置文件（config.json）
 *   2. 初始化级联流水线（CascadePipeline）
 *   3. 打开视频源（文件/摄像头/RTSP）
 *   4. 主循环：读帧 → 推理 → 显示
 *   5. 清理资源
 *
 * 编译：
 *   ./build-linux_RK3588.sh
 *
 * 运行：
 *   ./install/rknn_yolov5_demo_Linux/rknn_yolov5_demo <model.rknn> <video>
 *   例如：
 *   ./rknn_yolov5_demo ./model/RK3588/yolov5s-640-640.rknn 0
 *   ./rknn_yolov5_demo ./model/RK3588/yolov5s-640-640.rknn test.mp4
 */

#include <stdio.h>
#include <cstring>
#include <memory>
#include <sys/time.h>

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "rkYolov5s.hpp"
#include "rknnPool.hpp"
#include "config_loader.hpp"
#include "postprocess.h"
#include "pipeline/CascadePipeline.hpp"

// P1: 使用彩色日志系统
#include "Logger.hpp"

/**
 * 程序入口
 *
 * @param argc 参数数量（必须为3：程序名 + 模型路径 + 视频源）
 * @param argv 参数数组
 * @return 0 成功，-1 失败
 */
int main(int argc, char **argv)
{
    char *model_name = NULL;

    // 检查命令行参数：程序名 + 模型路径 + 视频/摄像头
    if (argc != 3)
    {
        LOG_ERROR("[Main]", "Usage: %s <rknn model> <video/camera>", argv[0]);
        return -1;
    }

    // 参数1：模型文件路径
    model_name = (char *)argv[1];
    // 参数2：视频文件路径或摄像头序号（0, 1, 2...）
    char *vedio_name = argv[2];

    // 加载配置文件（config.json）
    AppConfig config = load_config();
    config.model_path = model_name;
    config.resolve_label_path();  // 根据模型路径解析标签文件路径

    // 初始化标签路径（供后处理模块使用）
    initLabelPath(model_name);

    // 初始化级联流水线
    ModelConfig mc;
    mc.path = model_name;           // 模型文件路径
    mc.input_width = config.input_width;   // 输入宽度（默认640）
    mc.input_height = config.input_height; // 输入高度（默认640）
    mc.thread_num = config.thread_num;     // NPU线程数（默认3）
    mc.type = "yolov5";             // 模型类型

    CascadePipeline pipeline;
    if (pipeline.init({mc}) != 0)
    {
        LOG_ERROR("[Main]", "CascadePipeline init fail!");
        return -1;
    }

    // 创建 OpenCV 显示窗口
    cv::namedWindow("Camera FPS");

    // 打开视频源（支持文件/摄像头/RTSP）
    cv::VideoCapture capture;
    int camera_id = atoi(vedio_name);
    if (camera_id > 0 || strcmp(vedio_name, "0") == 0)
        // 参数可解析为数字，认为是摄像头序号
        capture.open(camera_id);
    else
        // 否则认为是视频文件路径或 RTSP URL
        capture.open(vedio_name);

    if (!capture.isOpened())
    {
        LOG_ERROR("[Main]", "Failed to open video/camera: %s", vedio_name);
        return -1;
    }

    // 记录开始时间，用于计算总平均帧率
    struct timeval time;
    gettimeofday(&time, nullptr);
    auto startTime = time.tv_sec * 1000 + time.tv_usec / 1000;

    int frames = 0;              // 已处理帧数
    auto beforeTime = startTime; // 上一轮帧率统计的起始时间
    double currentFps = 0.0;    // 当前实时帧率

    // ========== 主检测循环 ==========
    while (capture.isOpened())
    {
        cv::Mat img;
        // 读取一帧图像
        if (capture.read(img) == false)
            break;

        // 将图像放入流水线进行异步推理
        // put() 是非阻塞的，会立即返回，推理在后台线程中进行
        if (pipeline.put(img) != 0)
            break;

        // 等待推理结果
        // 前 thread_num 帧不需要等待，因为线程池还在填充中
        // get() 是阻塞的，会等待直到对应的推理完成
        if (frames >= config.thread_num && pipeline.get(img) != 0)
            break;

        // 每 30 帧计算一次实时帧率
        if (frames % 30 == 0 && frames > 0)
        {
            gettimeofday(&time, nullptr);
            auto currentTime = time.tv_sec * 1000 + time.tv_usec / 1000;
            currentFps = 30.0 / float(currentTime - beforeTime) * 1000.0;
            beforeTime = currentTime;
        }

        // 在画面左上角显示帧率（绿色文字）
        char fpsText[32];
        snprintf(fpsText, sizeof(fpsText), "FPS: %.2f", currentFps);
        cv::putText(img, fpsText, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);

        // 显示处理后的图像
        cv::imshow("Camera FPS", img);
        if (cv::waitKey(1) == 'q') // 延时 1 毫秒，按 q 键退出
            break;

        frames++;
    }

    // ========== 清理：排空流水线中剩余的帧 ==========
    while (true)
    {
        cv::Mat img;
        if (pipeline.get(img) != 0)
            break;
        cv::imshow("Camera FPS", img);
        if (cv::waitKey(1) == 'q')
            break;
        frames++;
    }

    // 计算并打印总平均帧率
    gettimeofday(&time, nullptr);
    auto endTime = time.tv_sec * 1000 + time.tv_usec / 1000;
    if (endTime > startTime)
        LOG_INFO("[Main]", "Average: %f fps/s", float(frames) / float(endTime - startTime) * 1000.0);
    else
        LOG_INFO("[Main]", "Average: 0 fps/s");

    return 0;
}
