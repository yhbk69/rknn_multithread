#include "pipeline/CascadePipeline.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include <cstring>

// P1 修复: 使用帧池优化内存分配
#include "FramePool.hpp"

// 全局帧池实例（用于 CascadePipeline）
static FramePool g_frame_pool(32);

CascadePipeline::CascadePipeline() {}

CascadePipeline::~CascadePipeline() {}

int CascadePipeline::init(const std::vector<ModelConfig> &models, int channel_id)
{
    if (models.empty()) {
        fprintf(stderr, "[Cascade] Empty models\n");
        return -1;
    }

    is_cascade_ = (models.size() > 1);
    stages_.clear();
    while (!roi_counts_.empty()) roi_counts_.pop();

    for (size_t i = 0; i < models.size(); i++) {
        const auto &mc = models[i];
        std::unique_ptr<IStage> stage;

        if (mc.type == "yolo11") {
            auto ts = std::make_unique<TypedStage<YOLO11Engine>>(
                mc.path, mc.thread_num, channel_id);
            stage = std::move(ts);
        } else {
            /* 默认 yolov5 */
            auto ts = std::make_unique<TypedStage<YOLOv5Engine>>(
                mc.path, mc.thread_num, channel_id);
            stage = std::move(ts);
        }

        stage->name = mc.name.empty() ? "stage_" + std::to_string(i) : mc.name;
        stage->type = mc.type;
        stage->draw_result = mc.draw_result;
        stage->input_w = mc.input_width;
        stage->input_h = mc.input_height;
        stage->roi_class_names = mc.roi_class_names;

        /* 查找 ROI 来源 stage */
        if (!mc.roi_from.empty()) {
            for (size_t j = 0; j < i; j++) {
                if (stages_[j]->name == mc.roi_from) {
                    stage->roi_stage_idx = (int)j;
                    break;
                }
            }
        }

        int ret = stage->init(channel_id);
        if (ret != 0) {
            fprintf(stderr, "[Cascade] Stage %zu init failed\n", i);
            return ret;
        }

        if (stage->roi_stage_idx >= 0)
            printf("[Cascade] Stage %zu (%s) uses ROI from stage %d\n",
                   i, stage->name.c_str(), stage->roi_stage_idx);

        stages_.push_back(std::move(stage));
    }

    printf("[Cascade] %zu stage(s) initialized, cascade=%d\n",
           stages_.size(), (int)is_cascade_);
    return 0;
}

int CascadePipeline::put(const cv::Mat &frame)
{
    if (stages_.empty()) return -1;

    /* P1 修复: 使用帧池替代 cv::Mat::clone()，减少堆分配 */
    {
        std::lock_guard<std::mutex> lk(orig_mtx_);
        // 从帧池获取缓冲区并复制数据
        cv::Mat pooled_frame = g_frame_pool.acquire(frame.rows, frame.cols, frame.type());
        frame.copyTo(pooled_frame);
        orig_frames_.push(pooled_frame);
    }

    return stages_[0]->put(frame);
}

int CascadePipeline::get(cv::Mat &output)
{
    if (stages_.empty()) return 1;

    /* 1. 获取 Stage 0 结果 */
    cv::Mat frame_out;
    int ret = stages_[0]->get(frame_out);
    if (ret != 0) return ret;

    /* 2. 获取此帧对应的原始帧 */
    cv::Mat orig_frame;
    {
        std::lock_guard<std::mutex> lk(orig_mtx_);
        if (!orig_frames_.empty()) {
            orig_frame = orig_frames_.front();
            orig_frames_.pop();
        }
    }
    if (orig_frame.empty()) orig_frame = frame_out;

    /* 3. 逐 stage 处理（stage 0 已处理完，从 stage 1 开始） */
    last_detect_result_ = stages_[0]->getLastDetectResult();
    cv::Mat current_frame = frame_out;

    for (size_t si = 1; si < stages_.size(); si++) {
        auto &stage = stages_[si];

        /* ROI 模式 */
        if (stage->roi_stage_idx >= 0) {
            detect_result_group_t src_detect;
            if (stage->roi_stage_idx == 0) {
                src_detect = stages_[0]->getLastDetectResult();
            } else {
                src_detect = stages_[stage->roi_stage_idx]->getLastDetectResult();
            }

            const cv::Mat *src_frame = &current_frame;
            if (stage->roi_stage_idx == 0 && !orig_frame.empty())
                src_frame = &orig_frame;

            /* 筛选 ROI：按类名匹配 */
            std::vector<detect_result_t> rois;
            for (int i = 0; i < src_detect.count; i++) {
                auto &d = src_detect.results[i];
                if (stage->roi_class_names.empty()) {
                    rois.push_back(d);
                } else {
                    for (const auto &cls : stage->roi_class_names) {
                        if (cls == d.name) { rois.push_back(d); break; }
                    }
                }
            }

            int n_rois = (int)rois.size();

            /* P1 修复: 使用帧池进行 ROI 裁剪 */
            int actually_put = 0;
            std::vector<std::pair<int,int>> roi_offsets;
            for (auto &roi : rois) {
                int x1 = std::max(0, roi.box.left);
                int y1 = std::max(0, roi.box.top);
                int x2 = std::min(src_frame->cols - 1, roi.box.right);
                int y2 = std::min(src_frame->rows - 1, roi.box.bottom);
                int w = x2 - x1 + 1;
                int h = y2 - y1 + 1;
                if (w < 4 || h < 4) continue;

                // 从帧池获取缓冲区进行裁剪
                cv::Mat crop = g_frame_pool.acquire(w, h, src_frame->type());
                (*src_frame)(cv::Rect(x1, y1, w, h)).copyTo(crop);

                cv::Mat resized;
                cv::resize(crop, resized, cv::Size(stage->input_w, stage->input_h));
                stage->put(resized);
                roi_offsets.push_back({x1, y1});
                actually_put++;

                // 释放缓冲区回帧池
                g_frame_pool.release(crop);
            }
            {
                std::lock_guard<std::mutex> lk(roi_mtx_);
                roi_counts_.push(actually_put);
            }

            /* 收集结果 */
            // P0 修复: 初始化 expected 防止未定义行为
            int expected = 0;
            {
                std::lock_guard<std::mutex> lk(roi_mtx_);
                if (!roi_counts_.empty()) {
                    expected = roi_counts_.front();
                    roi_counts_.pop();
                }
            }

            detect_result_group_t merged;
            merged.count = 0;

            /* 保留非 ROI_class 结果 */
            if (actually_put > 0 && !stage->roi_class_names.empty()) {
                for (int i = 0; i < src_detect.count && merged.count < OBJ_NUMB_MAX_SIZE; i++) {
                    bool is_roi = false;
                    for (const auto &cls : stage->roi_class_names) {
                        if (cls == src_detect.results[i].name) { is_roi = true; break; }
                    }
                    if (!is_roi)
                        merged.results[merged.count++] = src_detect.results[i];
                }
            } else if (actually_put == 0) {
                merged = src_detect;
            }

            /* 收集 stage 结果并绘制 */
            for (int i = 0; i < expected; i++) {
                cv::Mat stage_out;
                if (stage->get(stage_out) != 0) break;
                auto stage_detect = stage->getLastDetectResult();

                for (int j = 0; j < stage_detect.count && merged.count < OBJ_NUMB_MAX_SIZE; j++)
                    merged.results[merged.count++] = stage_detect.results[j];

                if (stage_out.data && stage->draw_result) {
                    int roi_ox = (i < (int)roi_offsets.size()) ? roi_offsets[i].first : 0;
                    int roi_oy = (i < (int)roi_offsets.size()) ? roi_offsets[i].second : 0;
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
            /* 非 ROI 模式：全图推理 */
            stage->put(orig_frame.empty() ? current_frame : orig_frame);
            cv::Mat stage_out;
            stage->get(stage_out);
            last_detect_result_ = stage->getLastDetectResult();
            current_frame = stage_out;
        }
    }

    last_frame_ = current_frame;
    output = current_frame;

    // P1 修复: 释放原始帧回帧池
    if (!orig_frame.empty()) {
        g_frame_pool.release(orig_frame);
    }

    return 0;
}

void CascadePipeline::set_thresholds(float conf, float nms)
{
    for (auto &s : stages_)
        s->setThresholds(conf, nms);
}
