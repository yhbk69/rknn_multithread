#ifndef ROI_PROCESSOR_HPP
#define ROI_PROCESSOR_HPP

#include <vector>
#include <string>
#include <utility>
#include <opencv2/core.hpp>
#include "postprocess.h"

class FramePool;

struct RoiCropResult {
    std::vector<cv::Mat> crops;
    std::vector<std::pair<int, int>> offsets;
    int count = 0;
};

class RoiProcessor {
public:
    RoiProcessor() = default;

    std::vector<detect_result_t> filterByClassNames(
        const detect_result_group_t& src_detect,
        const std::vector<std::string>& roi_class_names);

    RoiCropResult cropRois(
        const cv::Mat& src_frame,
        const std::vector<detect_result_t>& rois,
        int target_w, int target_h,
        FramePool& pool);

    detect_result_group_t mergeResults(
        const detect_result_group_t& src_detect,
        const std::vector<std::string>& roi_class_names,
        bool has_rois);

    void restoreCoordinates(
        detect_result_group_t& result,
        int roi_x, int roi_y);
};

#endif // ROI_PROCESSOR_HPP
