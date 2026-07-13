/*
 * CascadePipeline.cpp - 多模型级联检测流水线实现
 *
 * 支持两种工作模式：
 *   1. 单模型模式（models.size()==1）：等价于 rknnPool，直接推理返回
 *   2. 级联模式（models.size()>1）：Stage 0 检测 → ROI 裁剪 → Stage 1 二次检测
 *
 * ROI 级联流程：
 *   Stage 0 全图检测 → 按类别过滤 ROI → 裁剪 ROI 区域
 *     → Stage 1 二次检测（如：person→细粒度 PPE 检测）
 *     → 合并结果 → 坐标还原到全图
 *
 * 帧同步：
 *   orig_frames_ 队列与 stages_[0] 的 put/get 一一对应，
 *   确保 ROI 裁剪时能取到正确的原始帧。
 */

#include "pipeline/CascadePipeline.hpp"
#include "pipeline/RoiProcessor.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include <cstring>

#include "FramePool.hpp"
#include "Logger.hpp"

// 全局帧池：预分配 32 个 cv::Mat 缓冲区，避免每帧动态分配
static FramePool g_frame_pool(32);

// 全局 ROI 处理器：提供过滤、裁剪、合并功能
static RoiProcessor g_roi_processor;

CascadePipeline::CascadePipeline() {}
CascadePipeline::~CascadePipeline() {}

/**
 * 初始化流水线
 *
 * 根据模型配置列表创建 Stage，每个 Stage 包装一个 rknnPool 线程池。
 *
 * @param models     模型配置列表（按 stage 顺序排列）
 * @param channel_id 通道编号（用于 NPU 核心分配，多通道均匀分布）
 * @return 0 成功，-1 失败
 */
int CascadePipeline::init(const std::vector<ModelConfig> &models, int channel_id)
{
    if (models.empty()) {
        LOG_ERROR("[Cascade]", "Empty models");
        return -1;
    }

    // 判断是否为级联模式（多个模型）
    is_cascade_ = (models.size() > 1);
    stages_.clear();
    while (!roi_counts_.empty()) roi_counts_.pop();  // 清空 ROI 计数队列

    // 逐个创建 Stage
    for (size_t i = 0; i < models.size(); i++) {
        const auto &mc = models[i];
        std::unique_ptr<IStage> stage;

        // 根据模型类型创建对应的 TypedStage
        if (mc.type == "yolo11") {
            // YOLO11 引擎：anchor-free，单输出张量
            auto ts = std::make_unique<TypedStage<YOLO11Engine>>(
                mc.path, mc.thread_num, channel_id);
            stage = std::move(ts);
        } else {
            /* 默认 YOLOv5 引擎：anchor-based，三尺度输出 */
            auto ts = std::make_unique<TypedStage<YOLOv5Engine>>(
                mc.path, mc.thread_num, channel_id);
            stage = std::move(ts);
        }

        // 设置 Stage 元数据
        stage->name = mc.name.empty() ? "stage_" + std::to_string(i) : mc.name;
        stage->type = mc.type;
        stage->draw_result = mc.draw_result;
        stage->input_w = mc.input_width;
        stage->input_h = mc.input_height;
        stage->roi_class_names = mc.roi_class_names;

        /* 查找 ROI 来源 stage（用于级联 ROI 裁剪） */
        if (!mc.roi_from.empty()) {
            for (size_t j = 0; j < i; j++) {
                if (stages_[j]->name == mc.roi_from) {
                    stage->roi_stage_idx = (int)j;
                    break;
                }
            }
        }

        // 初始化 Stage（内部创建 rknnPool 线程池，加载 RKNN 模型）
        int ret = stage->init(channel_id);
        if (ret != 0) {
            LOG_ERROR("[Cascade]", "Stage %zu init failed", i);
            return ret;
        }

        if (stage->roi_stage_idx >= 0)
            LOG_INFO("[Cascade]", "Stage %zu (%s) uses ROI from stage %d",
                   i, stage->name.c_str(), stage->roi_stage_idx);

        stages_.push_back(std::move(stage));
    }

    LOG_INFO("[Cascade]", "%zu stage(s) initialized, cascade=%d",
           stages_.size(), (int)is_cascade_);
    return 0;
}

/**
 * 提交一帧到流水线（非阻塞）
 *
 * 流程：
 *   1. 从帧池获取缓冲区，复制原图（避免 clone 堆分配）
 *   2. 将原图存入 orig_frames_ 队列（与 get() 一一对应）
 *   3. 提交到 Stage 0 的 rknnPool 任务队列
 *
 * @param frame 输入帧（BGR 格式）
 * @return 0 成功
 */
int CascadePipeline::put(const cv::Mat &frame)
{
    if (stages_.empty()) return -1;

    // 使用帧池替代 cv::Mat::clone()，减少堆分配
    {
        std::lock_guard<std::mutex> lk(orig_mtx_);
        // 从帧池获取缓冲区并复制数据
        cv::Mat pooled_frame = g_frame_pool.acquire(frame.rows, frame.cols, frame.type());
        frame.copyTo(pooled_frame);
        orig_frames_.push(pooled_frame);
    }

    // 提交到 Stage 0 推理（非阻塞，立即返回）
    return stages_[0]->put(frame);
}

/**
 * 获取推理结果（阻塞，等待 NPU 完成）
 *
 * 流程：
 *   1. 从 Stage 0 获取推理结果
 *   2. 取出对应的原始帧
 *   3. 逐 Stage 处理（级联模式下从 Stage 1 开始）
 *   4. 合并所有 Stage 的检测结果
 *   5. 释放原始帧回帧池
 *
 * @param output 输出帧（已绘制检测框）
 * @return 0 成功，1 队列为空
 */
int CascadePipeline::get(cv::Mat &output)
{
    if (stages_.empty()) return 1;

    // ========== 步骤1：获取 Stage 0 结果 ==========
    cv::Mat frame_out;
    int ret = stages_[0]->get(frame_out);
    if (ret != 0) return ret;

    // ========== 步骤2：取出对应的原始帧 ==========
    // orig_frames_ 与 stages_[0] 的 put/get 一一对应
    cv::Mat orig_frame;
    {
        std::lock_guard<std::mutex> lk(orig_mtx_);
        if (!orig_frames_.empty()) {
            orig_frame = orig_frames_.front();
            orig_frames_.pop();
        }
    }
    if (orig_frame.empty()) orig_frame = frame_out;

    // ========== 步骤3：逐 Stage 处理 ==========
    last_detect_result_ = stages_[0]->getLastDetectResult();
    cv::Mat current_frame = frame_out;

    for (size_t si = 1; si < stages_.size(); si++) {
        auto &stage = stages_[si];

        /* --- ROI 模式：从上游 Stage 裁剪 ROI 区域进行二次检测 --- */
        if (stage->roi_stage_idx >= 0) {
            // 获取上游 Stage 的检测结果
            detect_result_group_t src_detect;
            if (stage->roi_stage_idx == 0) {
                src_detect = stages_[0]->getLastDetectResult();
            } else {
                src_detect = stages_[stage->roi_stage_idx]->getLastDetectResult();
            }

            // 选择源帧（Stage 0 使用原始帧，其他 Stage 使用当前帧）
            const cv::Mat *src_frame = &current_frame;
            if (stage->roi_stage_idx == 0 && !orig_frame.empty())
                src_frame = &orig_frame;

            // 按类别名过滤 ROI（如：只保留 "person" 类别的检测框）
            auto rois = g_roi_processor.filterByClassNames(src_detect, stage->roi_class_names);

            // 裁剪 ROI 区域，统一缩放到 Stage 的输入尺寸
            auto crop_result = g_roi_processor.cropRois(
                *src_frame, rois, stage->input_w, stage->input_h, g_frame_pool);

            // 将裁剪后的 ROI 图像提交到 Stage 推理
            for (auto& crop : crop_result.crops)
                stage->put(crop);

            int actually_put = crop_result.count;

            // 记录本帧提交的 ROI 数量（用于 get() 时知道要取多少结果）
            {
                std::lock_guard<std::mutex> lk(roi_mtx_);
                roi_counts_.push(actually_put);
            }

            // 获取上游帧对应的 ROI 数量
            int expected = 0;
            {
                std::lock_guard<std::mutex> lk(roi_mtx_);
                if (!roi_counts_.empty()) {
                    expected = roi_counts_.front();
                    roi_counts_.pop();
                }
            }

            // 保留上游检测结果中非 ROI 类别的检测框
            auto merged = g_roi_processor.mergeResults(
                src_detect, stage->roi_class_names, actually_put > 0);

            // 逐个获取 ROI 检测结果并合并
            for (int i = 0; i < expected; i++) {
                cv::Mat stage_out;
                if (stage->get(stage_out) != 0) break;
                auto stage_detect = stage->getLastDetectResult();

                // 将 Stage 检测结果追加到 merged（坐标已还原）
                for (int j = 0; j < stage_detect.count && merged.count < OBJ_NUMB_MAX_SIZE; j++)
                    merged.results[merged.count++] = stage_detect.results[j];

                // 在帧上绘制 Stage 检测框
                if (stage_out.data && stage->draw_result) {
                    int roi_ox = (i < (int)crop_result.offsets.size()) ? crop_result.offsets[i].first : 0;
                    int roi_oy = (i < (int)crop_result.offsets.size()) ? crop_result.offsets[i].second : 0;
                    char text[256];
                    for (int j = 0; j < stage_detect.count; j++) {
                        auto *det = &stage_detect.results[j];
                        snprintf(text, sizeof(text), "[%s] %s %.1f%%",
                                 stage->name.c_str(), det->name, det->prop * 100);
                        cv::rectangle(current_frame,
                                      cv::Point(det->box.left + roi_ox, det->box.top + roi_oy),
                                      cv::Point(det->box.right + roi_ox, det->box.bottom + roi_oy),
                                      cv::Scalar(0, 255, 0), 2);
                        cv::putText(current_frame, text,
                                    cv::Point(det->box.left + roi_ox, det->box.top + roi_oy - 5),
                                    cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 255, 0));
                    }
                }
            }
            last_detect_result_ = merged;

        } else {
            /* --- 非 ROI 模式：全图推理 --- */
            stage->put(orig_frame.empty() ? current_frame : orig_frame);
            cv::Mat stage_out;
            stage->get(stage_out);
            last_detect_result_ = stage->getLastDetectResult();
            current_frame = stage_out;
        }
    }

    last_frame_ = current_frame;
    output = current_frame;

    // 释放原始帧回帧池（供后续帧重用）
    if (!orig_frame.empty()) {
        g_frame_pool.release(orig_frame);
    }

    return 0;
}

/**
 * 动态设置所有 Stage 的检测阈值
 * @param conf 置信度阈值
 * @param nms  NMS IoU 阈值
 */
void CascadePipeline::set_thresholds(float conf, float nms)
{
    for (auto &s : stages_)
        s->setThresholds(conf, nms);
}

/**
 * 获取 Stage 0 队列中的待处理帧数
 * 用于跳帧决策：当积压严重时丢弃新帧
 */
int CascadePipeline::pendingCount() const
{
    if (stages_.empty()) return 0;
    return stages_[0]->pendingCount();
}
