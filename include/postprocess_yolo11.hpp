#ifndef POSTPROCESS_YOLO11_H
#define POSTPROCESS_YOLO11_H

#include <stdint.h>
#include <vector>
#include <string>
#include "config.h"
#include "postprocess.h"

/*
 * YOLO11PostProcess — YOLO11 anchor-free 后处理
 *
 * YOLO11 输出为单张量，形状 [1, 8400, 84]（NHWC 布局）：
 *   8400 = 80*80 + 40*40 + 20*20（三个 stride 级联）
 *   84   = 4 (cx,cy,w,h) + 80 类概率
 * 无 objectness 分支，无预定义 anchor。
 */
class YOLO11PostProcess {
public:
    YOLO11PostProcess();
    ~YOLO11PostProcess();

    int init(const char* model_path);
    void deinit();
    bool isInitialized() const { return initialized_; }

    /*
     * 处理单个输出张量
     * @param output       int8 输出数据
     * @param model_h,model_w  模型输入尺寸
     * @param conf_threshold   置信度阈值
     * @param nms_threshold    NMS IoU 阈值
     * @param pads              letterbox 填充
     * @param scale_w,scale_h  缩放比例
     * @param zp,scale          量化参数
     * @param group             输出结果
     */
    int process(int8_t *output, int model_h, int model_w,
                float conf_threshold, float nms_threshold,
                BOX_RECT pads, float scale_w, float scale_h,
                int32_t zp, float scale,
                detect_result_group_t *group);

private:
    bool initialized_ = false;
    int class_num_;
    char label_path_[512];
    char *labels_[OBJ_CLASS_NUM];
};

#endif
