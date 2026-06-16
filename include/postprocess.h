/*
 * postprocess.h - YOLOv5 目标检测后处理模块
 *
 * 支持多模型实例：每个 rkYolov5s 持有独立的 PostProcessContext
 */

#ifndef _RKNN_YOLOV5_DEMO_POSTPROCESS_H_
#define _RKNN_YOLOV5_DEMO_POSTPROCESS_H_

#include <stdint.h>
#include <vector>
#include <string>
#include "config.h"

typedef struct _BOX_RECT
{
    int left;
    int right;
    int top;
    int bottom;
} BOX_RECT;

typedef struct __detect_result_t
{
    char name[OBJ_NAME_MAX_SIZE];
    BOX_RECT box;
    float prop;
} detect_result_t;

typedef struct _detect_result_group_t
{
    int id;
    int count;
    detect_result_t results[OBJ_NUMB_MAX_SIZE];
} detect_result_group_t;

/* 后处理上下文：封装模型相关的标签和 anchor 状态 */
class PostProcessContext
{
public:
    PostProcessContext();
    ~PostProcessContext();

    /* 初始化：加载标签和 anchor 参数 */
    int init(const char* model_path);
    void deinit();

    /* 执行后处理 */
    int process(int8_t *input0, int8_t *input1, int8_t *input2,
                int model_in_h, int model_in_w,
                float conf_threshold, float nms_threshold,
                BOX_RECT pads, float scale_w, float scale_h,
                std::vector<int32_t> &qnt_zps, std::vector<float> &qnt_scales,
                detect_result_group_t *group);

    bool isInitialized() const { return initialized_; }

private:
    bool initialized_;
    char label_path_[512];
    int anchor_small_[6];
    int anchor_medium_[6];
    int anchor_large_[6];
    int class_num_;
    char *labels_[OBJ_CLASS_NUM];
};

/* ============ 旧 C API（向后兼容，使用全局上下文） ============ */
void initLabelPath(const char* model_path);
void deinitPostProcess();
int post_process(int8_t *input0, int8_t *input1, int8_t *input2,
                 int model_in_h, int model_in_w,
                 float conf_threshold, float nms_threshold,
                 BOX_RECT pads, float scale_w, float scale_h,
                 std::vector<int32_t> &qnt_zps, std::vector<float> &qnt_scales,
                 detect_result_group_t *group);

#endif //_RKNN_YOLOV5_DEMO_POSTPROCESS_H_
