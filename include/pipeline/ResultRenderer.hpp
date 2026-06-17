#ifndef PIPELINE_RESULT_RENDERER_HPP
#define PIPELINE_RESULT_RENDERER_HPP

#include <cstdio>
#include <opencv2/opencv.hpp>
#include <QImage>

/*
 * ResultRenderer — 帧渲染工具类
 *
 * 提供 FPS 文字绘制和 BGR→QImage 转换功能。
 * 线程安全：每个线程应持有独立实例（或使用 thread_local）。
 */
class ResultRenderer {
public:
    /*
     * 在帧上绘制 FPS 文字（左上角绿色大字）
     * @param frame  BGR 格式的输入帧（会被修改）
     * @param fps    当前帧率
     */
    void drawFps(cv::Mat& frame, double fps) {
        char text[32];
        snprintf(text, sizeof(text), "FPS: %.2f", fps);
        cv::putText(frame, text, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 255, 0), 2);
    }

    /*
     * 将 OpenCV BGR 帧转换为 QImage（RGB888 格式）
     * 注意：此函数会执行 BGR→RGB 颜色转换 + QImage 深拷贝
     *       如果调用方只需要 cv::Mat，应避免调用此函数
     *
     * @param bgr_frame  BGR 格式的输入帧
     * @return           RGB888 格式的 QImage（深拷贝，安全跨线程传递）
     */
    QImage toQImage(const cv::Mat& bgr_frame) {
        if (bgr_frame.empty()) return QImage();
        cv::Mat rgb;
        cv::cvtColor(bgr_frame, rgb, cv::COLOR_BGR2RGB);
        /* .copy() 做深拷贝，确保 QImage 不引用 cv::Mat 的内存 */
        return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
    }
};

#endif
