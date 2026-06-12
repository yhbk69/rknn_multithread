/**
 * @file preprocessor.hpp
 * @brief 图像预处理 — letterbox + 色彩转换 + 张量化
 *
 * 将原始图像转换为模型所需输入格式:
 * 1. Letterbox 缩放 (保持宽高比, 灰边填充)
 * 2. BGR → RGB
 * 3. 归一化到 [0,1]
 * 4. HWC → CHW
 */

#ifndef PREPROCESSOR_HPP
#define PREPROCESSOR_HPP

#include <vector>
#include <algorithm>
#include <cstring>
#include <opencv2/opencv.hpp>

namespace Preprocessor {

    /// Letterbox 缩放: 保持宽高比的等比缩放 + 灰边填充
    inline cv::Mat letterbox(const cv::Mat& img, int targetW, int targetH,
                             const cv::Scalar& fillColor = cv::Scalar(114, 114, 114)) {
        float scaleW = static_cast<float>(targetW) / img.cols;
        float scaleH = static_cast<float>(targetH) / img.rows;
        float scale = std::min(scaleW, scaleH);

        int newW = static_cast<int>(img.cols * scale);
        int newH = static_cast<int>(img.rows * scale);

        cv::Mat resized;
        cv::resize(img, resized, cv::Size(newW, newH), 0, 0, cv::INTER_LINEAR);

        cv::Mat padded(targetH, targetW, CV_8UC3, fillColor);
        int dx = (targetW - newW) / 2;
        int dy = (targetH - newH) / 2;
        resized.copyTo(padded(cv::Rect(dx, dy, newW, newH)));

        return padded;
    }

    /// 图像 → CHW float 张量 (BGR→RGB, 归一化到 [0,1])
    inline std::vector<float> imageToTensor(const cv::Mat& img) {
        cv::Mat rgb;
        cv::cvtColor(img, rgb, cv::COLOR_BGR2RGB);
        rgb.convertTo(rgb, CV_32FC3, 1.0 / 255.0);

        int C = rgb.channels();
        int H = rgb.rows;
        int W = rgb.cols;
        std::vector<float> tensor(C * H * W);

        std::vector<cv::Mat> channels(C);
        for (int c = 0; c < C; ++c) {
            channels[c] = cv::Mat(H, W, CV_32FC1, tensor.data() + c * H * W);
        }
        cv::split(rgb, channels);

        return tensor;
    }

} // namespace Preprocessor

#endif // PREPROCESSOR_HPP
