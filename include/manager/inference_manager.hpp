#ifndef INFERENCE_MANAGER_HPP
#define INFERENCE_MANAGER_HPP

#include <string>
#include <vector>
#include <functional>
#include <opencv2/opencv.hpp>
#include <QString>
#include "types.hpp"

class IEngine;

class InferenceManager {
public:
    struct Callbacks {
        std::function<void(const QString& cat, const QString& msg)> log;
    };

    InferenceManager();
    ~InferenceManager() = default;

    void setCallbacks(const Callbacks& cb) { callbacks_ = cb; }

    // --- 工具方法 ---

    static QString formatClassSummary(const std::vector<Detection>& detections);

    // --- 单张图片推理 ---
    struct ImageResult {
        cv::Mat annotated;          // BGR image with detections drawn
        std::vector<Detection> detections;
        double elapsedMs = 0;
        bool success = false;
        QString errorMsg;
    };

    ImageResult processImage(IEngine* engine,
                             const cv::Mat& imageBgr,
                             float confThreshold,
                             float nmsThreshold);

    // --- 文件夹批量推理 ---
    struct FolderResult {
        int succ = 0;
        int total = 0;
    };

    FolderResult processFolder(IEngine* engine,
                               const std::string& dirPath,
                               float confThreshold,
                               float nmsThreshold,
                               const std::string& outputDir,
                               std::function<void(int succ, int total)> onProgress);

private:
    Callbacks callbacks_;
};

#endif // INFERENCE_MANAGER_HPP
