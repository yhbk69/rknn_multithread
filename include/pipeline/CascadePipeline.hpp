#ifndef CASCADE_PIPELINE_HPP
#define CASCADE_PIPELINE_HPP

#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <queue>

#include "opencv2/core/core.hpp"

#include "core/IEngine.hpp"
#include "rknnPool.hpp"
#include "config_loader.hpp"
#include "postprocess.h"
#include "rkYolov5s.hpp"
#include "engine/YOLO11Engine.hpp"

/*
 * CascadePipeline — 多模型级联检测流水线
 *
 * 支持两种模式：
 *   1. 单模型模式：等价于 rknnPool，直接推理返回
 *   2. 级联模式：Stage 0 检测 → ROI 裁剪 → Stage 1 二次检测
 *
 * 线程安全：put() 和 get() 必须从同一线程调用（单生产者单消费者）
 *
 * 数据流：
 *   put(frame) → [Stage 0 推理] → get() → [ROI 裁剪 + Stage 1 推理] → 输出
 */
class CascadePipeline {
public:
    CascadePipeline();
    ~CascadePipeline();

    /*
     * 初始化流水线
     * @param models     模型配置列表（按 stage 顺序）
     * @param channel_id 通道编号，用于 NPU 核心分配
     */
    int init(const std::vector<ModelConfig> &models, int channel_id = 0);

    /*
     * 提交一帧到流水线（非阻塞）
     * 内部会 clone 原图存入 orig_frames_ 队列，与 get() 一一对应
     */
    int put(const cv::Mat &frame);

    /*
     * 获取推理结果（阻塞，等待 NPU 完成）
     * 内部自动处理 ROI 裁剪和多 stage 串联
     */
    int get(cv::Mat &output);

    detect_result_group_t getLastDetectResult() const {
        return last_detect_result_;
    }

    void set_thresholds(float conf, float nms);

    cv::Mat getLastFrame() const { return last_frame_; }
    bool isCascade() const { return is_cascade_; }

private:
    bool is_cascade_ = false;
    cv::Mat last_frame_;
    mutable detect_result_group_t last_detect_result_;

    /* 未绘制的原始帧队列（每帧对应一个 entry，与 pool get 顺序一致） */
    std::queue<cv::Mat> orig_frames_;
    std::mutex orig_mtx_;

    /* 类型擦除的 stage 基类 */
    struct IStage {
        std::string name;
        std::string type;
        bool draw_result = true;
        int input_w = 640, input_h = 640;
        std::vector<std::string> roi_class_names;
        int roi_stage_idx = -1;

        virtual ~IStage() = default;
        virtual int init(int channel_id) = 0;
        virtual int put(const cv::Mat &frame) = 0;
        virtual int get(cv::Mat &output) = 0;
        virtual detect_result_group_t getLastDetectResult() const = 0;
        virtual void setThresholds(float conf, float nms) = 0;
    };

    /* 具体 typed stage：包裹 rknnPool */
    template<typename EngineT>
    struct TypedStage : IStage {
        rknnPool<EngineT, cv::Mat, cv::Mat> pool;

        TypedStage(const std::string &mp, int tn, int cid)
            : pool(mp, tn, cid) {}

        int init(int) override { return pool.init(); }
        int put(const cv::Mat &f) override { return pool.put(f); }
        int get(cv::Mat &o) override { return pool.get(o); }

        detect_result_group_t getLastDetectResult() const override {
            return pool.getLastDetectResult();
        }

        void setThresholds(float c, float n) override {
            pool.set_thresholds(c, n);
        }
    };

    std::vector<std::unique_ptr<IStage>> stages_;

    /* 每帧预留给下游 stage 的 ROI 数量（用于跟踪批处理） */
    std::queue<int> roi_counts_;
    std::mutex roi_mtx_;
};

#endif
