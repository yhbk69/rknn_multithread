#include "inference_manager.hpp"
#include "inference_engine.hpp"
#include "preprocessor.hpp"
// no postprocessor.hpp yet
#include "config.hpp"

#include <filesystem>
#include <algorithm>
#include <chrono>

namespace fs = std::filesystem;

InferenceManager::InferenceManager() {
}

QString InferenceManager::formatClassSummary(const std::vector<Detection>& detections) {
    if (detections.empty()) return {};
    std::map<int, int> classCounts;
    for (const auto& det : detections) classCounts[det.class_id]++;
    QString detail;
    for (const auto& [cid, cnt] : classCounts) {
        if (!detail.isEmpty()) detail += ", ";
        detail += QString("%1x class_%2").arg(cnt).arg(cid);
    }
    return detail;
}

InferenceManager::ImageResult InferenceManager::processImage(
    IEngine* engine,
    const cv::Mat& imageBgr,
    float confThreshold,
    float nmsThreshold)
{
    ImageResult result;
    if (!engine || imageBgr.empty()) {
        result.success = false;
        result.errorMsg = engine ? QStringLiteral("输入图像为空") : QStringLiteral("引擎未加载");
        return result;
    }

    try {
        auto start = std::chrono::high_resolution_clock::now();

        cv::Mat processed = Preprocessor::letterbox(imageBgr, Config::INPUT_WIDTH, Config::INPUT_HEIGHT);
        std::vector<float> tensor = Preprocessor::imageToTensor(processed);

        std::vector<Detection> detections;
        engine->infer(tensor, detections, imageBgr.cols, imageBgr.rows,
                      confThreshold, nmsThreshold);

        auto end = std::chrono::high_resolution_clock::now();
        result.elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
        result.detections = std::move(detections);

        result.annotated = imageBgr.clone();
        // drawDetections stub - TODO

        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMsg = QString::fromUtf8(e.what());
    }

    return result;
}

InferenceManager::FolderResult InferenceManager::processFolder(
    IEngine* engine,
    const std::string& dirPath,
    float confThreshold,
    float nmsThreshold,
    const std::string& outputDir,
    std::function<void(int succ, int total)> onProgress)
{
    FolderResult result{0, 0};
    if (!engine || !fs::exists(dirPath)) return result;

    static const std::vector<std::string> extensions = {".jpg", ".jpeg", ".png", ".bmp"};

    try {
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (!fs::is_regular_file(entry)) continue;
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (std::find(extensions.begin(), extensions.end(), ext) == extensions.end())
                continue;

            try {
                cv::Mat img = cv::imread(entry.path().string());
                if (!img.empty()) {
                    auto imgResult = processImage(engine, img, confThreshold, nmsThreshold);
                    if (imgResult.success) {
                        cv::imwrite(outputDir + "/result_" + entry.path().filename().string(),
                                    imgResult.annotated);
                        result.succ++;
                    }
                }
            } catch (...) {}

            result.total++;
            if (onProgress) onProgress(result.succ, result.total);
        }
    } catch (const std::exception& e) {
        if (callbacks_.log)
            callbacks_.log("错误", QString::fromUtf8(e.what()));
    }

    return result;
}
