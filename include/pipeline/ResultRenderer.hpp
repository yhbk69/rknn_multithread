#ifndef PIPELINE_RESULT_RENDERER_HPP
#define PIPELINE_RESULT_RENDERER_HPP

#include <cstdio>
#include <opencv2/opencv.hpp>
#include <QImage>

class ResultRenderer {
public:
    void drawFps(cv::Mat& frame, double fps) {
        char text[32];
        snprintf(text, sizeof(text), "FPS: %.2f", fps);
        cv::putText(frame, text, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 255, 0), 2);
    }

    QImage toQImage(const cv::Mat& bgr_frame) {
        cv::Mat rgb;
        cv::cvtColor(bgr_frame, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
    }
};

#endif
