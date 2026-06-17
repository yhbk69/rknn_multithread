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
 * 多 stage 串联：Stage 0 检测出目标后，按 roi_class_ids 筛选，
 * 从原图裁剪 ROI 送入 Stage 1 进行二次检测。
 * 每个 stage 独立 rknnPool，通过有界队列串联。
 */
class CascadePipeline {
public:
    CascadePipeline();
    ~CascadePipeline();

    int init(const std::vector<ModelConfig> &models, int channel_id = 0);
    int put(const cv::Mat &frame);
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
