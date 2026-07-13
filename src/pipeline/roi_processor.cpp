/*
 * roi_processor.cpp - ROI（感兴趣区域）处理器实现
 *
 * 功能：
 *   - filterByClassNames: 按类别名过滤检测结果
 *   - cropRois: 裁剪 ROI 区域并缩放到目标尺寸
 *   - mergeResults: 合并上游检测结果（保留非 ROI 类别）
 *
 * 使用场景：
 *   级联检测模式下，Stage 0 检测到 person 后，
 *   裁剪 person 区域送入 Stage 1 进行细粒度检测（如 PPE 检测）
 */

#include "pipeline/RoiProcessor.hpp"
#include "FramePool.hpp"
#include <opencv2/imgproc.hpp>

/**
 * 按类别名过滤检测结果
 *
 * 从上游 Stage 的检测结果中，筛选出指定类别的检测框。
 *
 * @param src_detect     上游检测结果组
 * @param roi_class_names 需要保留的类别名列表（空列表表示保留所有）
 * @return 过滤后的检测框列表
 */
std::vector<detect_result_t> RoiProcessor::filterByClassNames(
    const detect_result_group_t& src_detect,
    const std::vector<std::string>& roi_class_names)
{
    std::vector<detect_result_t> rois;

    for (int i = 0; i < src_detect.count; i++) {
        auto& d = src_detect.results[i];

        if (roi_class_names.empty()) {
            // 未配置过滤类别，保留所有检测框
            rois.push_back(d);
        } else {
            // 检查类别是否在过滤列表中
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

/**
 * 裁剪 ROI 区域并缩放到目标尺寸
 *
 * 流程：
 *   1. 将检测框坐标裁剪到图像范围内（防止越界）
 *   2. 从帧池获取缓冲区，裁剪 ROI 子图
 *   3. 缩放到 Stage 的输入尺寸（如 640×640）
 *   4. 记录 ROI 在原图中的偏移量（用于坐标还原）
 *
 * @param src_frame  源帧（全图）
 * @param rois       需要裁剪的检测框列表
 * @param target_w   目标宽度（Stage 输入宽度）
 * @param target_h   目标高度（Stage 输入高度）
 * @param pool       帧池（用于内存复用）
 * @return 裁剪结果（包含裁剪后的图像列表和偏移量）
 */
RoiCropResult RoiProcessor::cropRois(
    const cv::Mat& src_frame,
    const std::vector<detect_result_t>& rois,
    int target_w, int target_h,
    FramePool& pool)
{
    RoiCropResult result;
    result.count = 0;

    for (auto& roi : rois) {
        // 将检测框坐标裁剪到图像范围内
        int x1 = std::max(0, roi.box.left);
        int y1 = std::max(0, roi.box.top);
        int x2 = std::min(src_frame.cols - 1, roi.box.right);
        int y2 = std::min(src_frame.rows - 1, roi.box.bottom);
        int w = x2 - x1 + 1;
        int h = y2 - y1 + 1;

        // 跳过太小的 ROI（宽度或高度 < 4 像素）
        if (w < 4 || h < 4) continue;

        // 从帧池获取缓冲区并裁剪 ROI
        cv::Mat crop = pool.acquire(w, h, src_frame.type());
        src_frame(cv::Rect(x1, y1, w, h)).copyTo(crop);

        // 缩放到 Stage 输入尺寸
        cv::Mat resized;
        cv::resize(crop, resized, cv::Size(target_w, target_h));

        // 释放临时缓冲区回帧池
        pool.release(crop);

        // 保存裁剪结果
        result.crops.push_back(resized);
        result.offsets.push_back({x1, y1});  // 记录 ROI 在原图中的偏移量
        result.count++;
    }

    return result;
}

/**
 * 合并上游检测结果
 *
 * 在级联模式下，保留上游检测结果中非 ROI 类别的检测框。
 * 例如：Stage 0 检测到 "person" 和 "car"，
 *       ROI 过滤只取 "person"，则 "car" 保留到最终结果。
 *
 * @param src_detect      上游检测结果
 * @param roi_class_names ROI 过滤的类别名列表
 * @param has_rois        是否有 ROI 被裁剪
 * @return 合并后的检测结果
 */
detect_result_group_t RoiProcessor::mergeResults(
    const detect_result_group_t& src_detect,
    const std::vector<std::string>& roi_class_names,
    bool has_rois)
{
    detect_result_group_t merged;
    merged.count = 0;

    if (has_rois && !roi_class_names.empty()) {
        // 有 ROI 被裁剪：保留非 ROI 类别的检测框
        for (int i = 0; i < src_detect.count && merged.count < OBJ_NUMB_MAX_SIZE; i++) {
            bool is_roi = false;
            for (const auto& cls : roi_class_names) {
                if (cls == src_detect.results[i].name) {
                    is_roi = true;
                    break;
                }
            }
            // 非 ROI 类别保留到最终结果
            if (!is_roi)
                merged.results[merged.count++] = src_detect.results[i];
        }
    } else if (!has_rois) {
        // 无 ROI 被裁剪：直接使用上游结果
        merged = src_detect;
    }

    return merged;
}
