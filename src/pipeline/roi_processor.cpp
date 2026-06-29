#include "pipeline/RoiProcessor.hpp"
#include "FramePool.hpp"
#include <opencv2/imgproc.hpp>

std::vector<detect_result_t> RoiProcessor::filterByClassNames(
    const detect_result_group_t& src_detect,
    const std::vector<std::string>& roi_class_names)
{
    std::vector<detect_result_t> rois;
    for (int i = 0; i < src_detect.count; i++) {
        auto& d = src_detect.results[i];
        if (roi_class_names.empty()) {
            rois.push_back(d);
        } else {
            for (const auto& cls : roi_class_names) {
                if (cls == d.name) {
                    rois.push_back(d);
                    break;
                }
            }
        }
    }
    return rois;
}

RoiCropResult RoiProcessor::cropRois(
    const cv::Mat& src_frame,
    const std::vector<detect_result_t>& rois,
    int target_w, int target_h,
    FramePool& pool)
{
    RoiCropResult result;
    result.count = 0;

    for (auto& roi : rois) {
        int x1 = std::max(0, roi.box.left);
        int y1 = std::max(0, roi.box.top);
        int x2 = std::min(src_frame.cols - 1, roi.box.right);
        int y2 = std::min(src_frame.rows - 1, roi.box.bottom);
        int w = x2 - x1 + 1;
        int h = y2 - y1 + 1;
        if (w < 4 || h < 4) continue;

        cv::Mat crop = pool.acquire(w, h, src_frame.type());
        src_frame(cv::Rect(x1, y1, w, h)).copyTo(crop);

        cv::Mat resized;
        cv::resize(crop, resized, cv::Size(target_w, target_h));

        pool.release(crop);

        result.crops.push_back(resized);
        result.offsets.push_back({x1, y1});
        result.count++;
    }

    return result;
}

detect_result_group_t RoiProcessor::mergeResults(
    const detect_result_group_t& src_detect,
    const std::vector<std::string>& roi_class_names,
    bool has_rois)
{
    detect_result_group_t merged;
    merged.count = 0;

    if (has_rois && !roi_class_names.empty()) {
        for (int i = 0; i < src_detect.count && merged.count < OBJ_NUMB_MAX_SIZE; i++) {
            bool is_roi = false;
            for (const auto& cls : roi_class_names) {
                if (cls == src_detect.results[i].name) {
                    is_roi = true;
                    break;
                }
            }
            if (!is_roi)
                merged.results[merged.count++] = src_detect.results[i];
        }
    } else if (!has_rois) {
        merged = src_detect;
    }

    return merged;
}
