/*
 * postprocess.h - YOLOv5 目标检测后处理模块的头文件
 *
 * 本文件定义了 YOLOv5 模型推理后的后处理相关数据结构与函数接口，
 * 包括特征图解码、置信度过滤、NMS 非极大值抑制等操作。
 */

#ifndef _RKNN_YOLOV5_DEMO_POSTPROCESS_H_
#define _RKNN_YOLOV5_DEMO_POSTPROCESS_H_

#include <stdint.h>
#include <vector>
#include "config.h"

/* 边界框矩形区域，用左、右、上、下四个坐标值表示 */
typedef struct _BOX_RECT
{
    int left;
    int right;
    int top;
    int bottom;
} BOX_RECT;

/* 单个检测结果，包含类别名称、边界框矩形区域和置信度 */
typedef struct __detect_result_t
{
    char name[OBJ_NAME_MAX_SIZE];  // 检测到的目标类别名称
    BOX_RECT box;                  // 目标边界框
    float prop;                    // 目标置信度（概率）
} detect_result_t;

/* 检测结果组，包含本帧所有检测到的目标 */
typedef struct _detect_result_group_t
{
    int id;                                      // 检测结果组 ID
    int count;                                   // 本帧检测到的目标数量
    detect_result_t results[OBJ_NUMB_MAX_SIZE];  // 存放所有检测结果的数组
} detect_result_group_t;

/*
 * 后处理函数：解码三个尺度的特征图、置信度过滤、NMS 非极大值抑制
 *
 * @param input0          第一个尺度（大特征图）的量化输出数据
 * @param input1          第二个尺度（中特征图）的量化输出数据
 * @param input2          第三个尺度（小特征图）的量化输出数据
 * @param model_in_h      模型输入高度
 * @param model_in_w      模型输入宽度
 * @param conf_threshold  置信度阈值，低于此值的框将被过滤
 * @param nms_threshold   NMS 的 IoU 阈值
 * @param pads            Letterbox 填充时产生的边距信息
 * @param scale_w         宽度方向的缩放比例
 * @param scale_h         高度方向的缩放比例
 * @param qnt_zps         各输出张量的量化零点（zero points）
 * @param qnt_scales      各输出张量的量化缩放因子（scales）
 * @param group           输出参数，存放最终的检测结果组
 * @return                成功返回 0，失败返回非零值
 */
int post_process(int8_t *input0, int8_t *input1, int8_t *input2, int model_in_h, int model_in_w,
                 float conf_threshold, float nms_threshold, BOX_RECT pads, float scale_w, float scale_h,
                 std::vector<int32_t> &qnt_zps, std::vector<float> &qnt_scales,
                 detect_result_group_t *group);

/* 释放后处理过程中分配的资源（如内部缓存等） */
void deinitPostProcess();

/* 设置标签文件路径（从模型路径推导） */
void initLabelPath(const char* model_path);

#endif //_RKNN_YOLOV5_DEMO_POSTPROCESS_H_
