/*
 * postprocess.cpp - YOLOv5 后处理模块实现
 *
 * 功能：将 NPU 输出的量化特征图解码为检测框
 *
 * 三尺度解码流程：
 *   1. 小尺度 (stride=8): 检测小目标，80×80 网格
 *   2. 中尺度 (stride=16): 检测中目标，40×40 网格
 *   3. 大尺度 (stride=32): 检测大目标，20×20 网格
 *
 * 每个网格单元预测 3 个 anchor：
 *   - box_confidence: 置信度（sigmoid）
 *   - class_probs: 类别概率（sigmoid）
 *   - box_xywh: 边界框（中心点+宽高）
 *
 * 后处理步骤：
 *   1. 量化反量化：int8 → float（公式：float = (int8 - zp) * scale）
 *   2. 置信度过滤：低于阈值的检测框丢弃
 *   3. NMS：按类别分组，IoU 高于阈值的框抑制
 *   4. 坐标还原：减去 padding，除以 scale 映射回原图
 *
 * 原始代码来自 Rockchip，已修改为面向对象的 PostProcessContext 类
 */

#include "postprocess.h"
#include "config_loader.hpp"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <set>
#include <vector>
#include <algorithm>
#include <numeric>

/* ============ PostProcessContext 实现 ============ */

/**
 * 构造函数：初始化成员变量
 */
PostProcessContext::PostProcessContext()
    : initialized_(false), class_num_(OBJ_CLASS_NUM)
{
    memset(label_path_, 0, sizeof(label_path_));
    memset(anchor_small_, 0, sizeof(anchor_small_));
    memset(anchor_medium_, 0, sizeof(anchor_medium_));
    memset(anchor_large_, 0, sizeof(anchor_large_));
    memset(labels_, 0, sizeof(labels_));
}

/**
 * 析构函数：释放标签内存
 */
PostProcessContext::~PostProcessContext()
{
    deinit();
}

/**
 * 初始化后处理上下文
 *
 * 流程：
 *   1. 从配置加载类别数
 *   2. 从模型路径推导标签文件路径
 *   3. 加载 anchor 参数
 *   4. 读取标签文件
 *
 * @param model_path 模型文件路径
 * @return 0 成功，-1 失败
 */
int PostProcessContext::init(const char* model_path)
{
    if (initialized_) return 0;  // 已初始化，直接返回

    // 从配置加载类别数
    AppConfig cfg = load_config();
    class_num_ = cfg.class_num;

    // 从模型路径推导标签路径
    // 例如：./model/RK3588/yolov5s.rknn → ./model/coco_80_labels_list.txt
    char* model_copy = strdup(model_path);
    char* dir = dirname(model_copy);
    snprintf(label_path_, sizeof(label_path_), "%s/../%s", dir, cfg.label_file.c_str());
    free(model_copy);

    // 加载 Anchor 参数（从 config.json）
    memcpy(anchor_small_, cfg.anchor_small, sizeof(anchor_small_));
    memcpy(anchor_medium_, cfg.anchor_medium, sizeof(anchor_medium_));
    memcpy(anchor_large_, cfg.anchor_large, sizeof(anchor_large_));

    // 加载标签文件（每行一个类别名）
    FILE *file = fopen(label_path_, "r");
    if (file == NULL)
    {
        printf("Open %s fail!\n", label_path_);
        return -1;
    }

    char *s = NULL;
    int n = 0;
    int i = 0;
    while (i < OBJ_CLASS_NUM)
    {
        int ch;
        size_t buf_cap = 256;
        s = (char *)malloc(buf_cap);
        if (!s) break;

        // 逐行读取标签（支持动态扩展缓冲区）
        int len = 0;
        while ((ch = fgetc(file)) != '\n' && ch != EOF)
        {
            if (len + 1 >= (int)buf_cap)
            {
                // 缓冲区不足，翻倍扩容
                buf_cap *= 2;
                void *tmp = realloc(s, buf_cap);
                if (tmp == NULL) { free(s); s = NULL; break; }
                s = (char *)tmp;
            }
            s[len++] = (char)ch;
        }
        if (ch == EOF && len == 0) { free(s); break; }
        s[len] = '\0';
        labels_[i++] = s;
    }
    fclose(file);

    printf("Using label path: %s (%d labels)\n", label_path_, i);
    initialized_ = true;
    return 0;
}

/**
 * 释放标签内存
 */
void PostProcessContext::deinit()
{
    if (!initialized_) return;

    for (int i = 0; i < class_num_; i++)
    {
        if (labels_[i] != nullptr)
        {
            free(labels_[i]);
            labels_[i] = nullptr;
        }
    }
    initialized_ = false;
}

/* ============ 内部辅助函数 ============ */

/**
 * 限制值在 [min, max] 范围内
 */
static inline int clamp_val(float val, int min, int max)
{
    return val > min ? (val < max ? val : max) : min;
}

/**
 * 计算两个框的 IoU（Intersection over Union）
 *
 * IoU = 交集面积 / 并集面积
 * 用于 NMS 中判断两个框是否重叠
 */
static float CalculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0,
                              float xmin1, float ymin1, float xmax1, float ymax1)
{
    // 计算交集面积
    float w = fmax(0.f, fmin(xmax0, xmax1) - fmax(xmin0, xmin1) + 1.0);
    float h = fmax(0.f, fmin(ymax0, ymax1) - fmax(ymin0, ymin1) + 1.0);
    float i = w * h;  // 交集

    // 计算并集面积
    float u = (xmax0 - xmin0 + 1.0) * (ymax0 - ymin0 + 1.0) +
              (xmax1 - xmin1 + 1.0) * (ymax1 - ymin1 + 1.0) - i;

    return u <= 0.f ? 0.f : (i / u);
}

/**
 * NMS（Non-Maximum Suppression，非极大值抑制）
 *
 * 对同一类别的检测框，按置信度排序，抑制 IoU 高于阈值的框。
 *
 * @param validCount     有效检测框数量
 * @param outputLocations 输出坐标（x, y, w, h 格式）
 * @param classIds       各框的类别 ID
 * @param order          按置信度排序的索引数组（被抑制的设为 -1）
 * @param filterId       当前处理的类别 ID
 * @param threshold      IoU 阈值（高于此值的框被抑制）
 */
static int nms(int validCount, std::vector<float> &outputLocations,
               std::vector<int> classIds, std::vector<int> &order,
               int filterId, float threshold)
{
    for (int i = 0; i < validCount; ++i)
    {
        // 跳过已抑制或类别不匹配的框
        if (order[i] == -1 || classIds[i] != filterId) continue;
        int n = order[i];

        for (int j = i + 1; j < validCount; ++j)
        {
            int m = order[j];
            if (m == -1 || classIds[j] != filterId) continue;

            // 将 (x, y, w, h) 转换为 (xmin, ymin, xmax, ymax)
            float xmin0 = outputLocations[n * 4 + 0];
            float ymin0 = outputLocations[n * 4 + 1];
            float xmax0 = outputLocations[n * 4 + 0] + outputLocations[n * 4 + 2];
            float ymax0 = outputLocations[n * 4 + 1] + outputLocations[n * 4 + 3];

            float xmin1 = outputLocations[m * 4 + 0];
            float ymin1 = outputLocations[m * 4 + 1];
            float xmax1 = outputLocations[m * 4 + 0] + outputLocations[m * 4 + 2];
            float ymax1 = outputLocations[m * 4 + 1] + outputLocations[m * 4 + 3];

            // 计算 IoU
            float iou = CalculateOverlap(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);
            if (iou > threshold) order[j] = -1;  // 抑制
        }
    }
    return 0;
}

/**
 * Sigmoid 激活函数
 * 将任意值映射到 (0, 1) 范围
 */
static inline float sigmoid(float x) { return 1.0f / (1.0f + expf(-x)); }

/**
 * 处理单个尺度的特征图
 *
 * 从量化 int8 特征图中解码检测框：
 *   1. 遍历每个网格单元
 *   2. 检查置信度是否超过阈值
 *   3. 解码 xywh 坐标（反量化 + 锚框缩放）
 *   4. 找到最大类别概率
 *   5. 过滤低置信度框
 *
 * @param input        输入特征图（int8 量化）
 * @param anchor       锚框数组（6个值：3组宽高）
 * @param grid_h       网格高度
 * @param grid_w       网格宽度
 * @param height       模型输入高度
 * @param width        模型输入宽度
 * @param stride       步长（8/16/32）
 * @param class_num    类别数
 * @param boxes        输出：检测框坐标
 * @param objProbs     输出：置信度
 * @param classId      输出：类别 ID
 * @param threshold    置信度阈值
 * @param zp           量化零点
 * @param scale        量化缩放因子
 * @return 有效检测框数量
 */
static int process_single_scale(int8_t *input, int *anchor, int grid_h, int grid_w,
                                int height, int width, int stride, int class_num,
                                std::vector<float> &boxes, std::vector<float> &objProbs,
                                std::vector<int> &classId, float threshold,
                                int32_t zp, float scale)
{
    int validCount = 0;
    int grid_len = grid_h * grid_w;
    float scale_inv = 1.0f / scale;

    // 遍历 3 个 anchor
    for (int a = 0; a < 3; a++)
    {
        // 遍历网格
        for (int i = 0; i < grid_h; i++)
        {
            for (int j = 0; j < grid_w; j++)
            {
                // 检查置信度（int8 反量化）
                int8_t box_confidence_i8 = input[(PROP_BOX_SIZE * a + 4) * grid_len + i * grid_w + j];
                float box_confidence = ((float)box_confidence_i8 - (float)zp) * scale;
                if (box_confidence < threshold) continue;  // 低于阈值，跳过

                // 解码边界框坐标
                int offset = (PROP_BOX_SIZE * a) * grid_len + i * grid_w + j;
                int8_t *in_ptr = input + offset;

                // 反量化 + sigmoid + 锚框缩放
                float box_x = (((float)*in_ptr - (float)zp) * scale) * 2.0f - 0.5f;
                float box_y = (((float)in_ptr[grid_len] - (float)zp) * scale) * 2.0f - 0.5f;
                float box_w = (((float)in_ptr[2 * grid_len] - (float)zp) * scale) * 2.0f;
                float box_h = (((float)in_ptr[3 * grid_len] - (float)zp) * scale) * 2.0f;

                // 转换为网格坐标
                box_x = (box_x + j) * (float)stride;
                box_y = (box_y + i) * (float)stride;
                // 缩放到锚框尺寸
                box_w = box_w * box_w * (float)anchor[a * 2];
                box_h = box_h * box_h * (float)anchor[a * 2 + 1];
                // 转换为左上角坐标
                box_x -= (box_w / 2.0f);
                box_y -= (box_h / 2.0f);

                // 找到最大类别概率
                int8_t maxClassProbs_i8 = in_ptr[5 * grid_len];
                int maxClassId = 0;
                for (int k = 1; k < class_num; ++k)
                {
                    int8_t prob = in_ptr[(5 + k) * grid_len];
                    if (prob > maxClassProbs_i8)
                    {
                        maxClassId = k;
                        maxClassProbs_i8 = prob;
                    }
                }

                // 检查类别概率是否超过阈值
                float maxClassProbs = ((float)maxClassProbs_i8 - (float)zp) * scale;
                if (maxClassProbs < threshold) continue;

                // 保存检测框
                objProbs.push_back(maxClassProbs * box_confidence);  // 最终置信度 = 类别概率 × 置信度
                classId.push_back(maxClassId);
                validCount++;
                boxes.push_back(box_x);
                boxes.push_back(box_y);
                boxes.push_back(box_w);
                boxes.push_back(box_h);
            }
        }
    }
    return validCount;
}

/* ============ PostProcessContext::process ============ */

/**
 * YOLOv5 后处理主函数
 *
 * 流程：
 *   1. 三尺度解码：小/中/大尺度特征图分别解码
 *   2. 合并所有尺度的检测框
 *   3. 按置信度排序
 *   4. 按类别执行 NMS
 *   5. 坐标还原到原图尺寸
 *
 * @param input0       小尺度特征图（stride=8）
 * @param input1       中尺度特征图（stride=16）
 * @param input2       大尺度特征图（stride=32）
 * @param model_in_h   模型输入高度
 * @param model_in_w   模型输入宽度
 * @param conf_threshold 置信度阈值
 * @param nms_threshold  NMS IoU 阈值
 * @param pads          letterbox 填充量
 * @param scale_w,scale_h 缩放比例
 * @param qnt_zps       量化零点数组
 * @param qnt_scales    量化缩放因子数组
 * @param group         输出检测结果组
 */
int PostProcessContext::process(int8_t *input0, int8_t *input1, int8_t *input2,
                                int model_in_h, int model_in_w,
                                float conf_threshold, float nms_threshold,
                                BOX_RECT pads, float scale_w, float scale_h,
                                std::vector<int32_t> &qnt_zps, std::vector<float> &qnt_scales,
                                detect_result_group_t *group)
{
    // 清零输出结构体
    memset(group, 0, sizeof(detect_result_group_t));

    // 预分配内存（避免频繁扩容）
    std::vector<float> filterBoxes;
    std::vector<float> objProbs;
    std::vector<int> classId;

    int stride0 = 8;
    // 预估检测框数量（三个尺度之和）
    int estimated = (model_in_h / stride0) * (model_in_w / stride0) * 3
                  + (model_in_h / 16) * (model_in_w / 16) * 3
                  + (model_in_h / 32) * (model_in_w / 32) * 3;
    filterBoxes.reserve(estimated * 4);
    objProbs.reserve(estimated);
    classId.reserve(estimated);

    // ========== 步骤1：三尺度解码 ==========
    // 小尺度：检测小目标
    int validCount0 = process_single_scale(input0, anchor_small_,
        model_in_h / stride0, model_in_w / stride0, model_in_h, model_in_w, stride0,
        class_num_, filterBoxes, objProbs, classId, conf_threshold, qnt_zps[0], qnt_scales[0]);

    // 中尺度：检测中目标
    int stride1 = 16;
    int validCount1 = process_single_scale(input1, anchor_medium_,
        model_in_h / stride1, model_in_w / stride1, model_in_h, model_in_w, stride1,
        class_num_, filterBoxes, objProbs, classId, conf_threshold, qnt_zps[1], qnt_scales[1]);

    // 大尺度：检测大目标
    int stride2 = 32;
    int validCount2 = process_single_scale(input2, anchor_large_,
        model_in_h / stride2, model_in_w / stride2, model_in_h, model_in_w, stride2,
        class_num_, filterBoxes, objProbs, classId, conf_threshold, qnt_zps[2], qnt_scales[2]);

    // 合并所有尺度的检测框
    int validCount = validCount0 + validCount1 + validCount2;
    if (validCount <= 0) return 0;  // 无检测结果

    // ========== 步骤2：按置信度排序 ==========
    std::vector<int> indexArray(validCount);
    std::iota(indexArray.begin(), indexArray.end(), 0);  // 生成 0,1,2,...,n-1

    std::sort(indexArray.begin(), indexArray.end(),
        [&objProbs](int a, int b) { return objProbs[a] > objProbs[b]; });  // 降序排序

    // ========== 步骤3：按类别执行 NMS ==========
    std::set<int> class_set(std::begin(classId), std::end(classId));
    for (auto c : class_set)
        nms(validCount, filterBoxes, classId, indexArray, c, nms_threshold);

    // ========== 步骤4：输出结果 ==========
    int last_count = 0;
    group->count = 0;

    for (int i = 0; i < validCount; ++i)
    {
        // 跳过已抑制的框或超出上限
        if (indexArray[i] == -1 || last_count >= OBJ_NUMB_MAX_SIZE) continue;
        int n = indexArray[i];

        // 从 (x, y, w, h) 转换为 (x1, y1, x2, y2)
        float x1 = filterBoxes[n * 4 + 0] - pads.left;   // 减去 padding
        float y1 = filterBoxes[n * 4 + 1] - pads.top;
        float x2 = x1 + filterBoxes[n * 4 + 2];
        float y2 = y1 + filterBoxes[n * 4 + 3];
        int id = classId[n];

        // 坐标还原到原图尺寸（除以缩放比例）
        group->results[last_count].box.left = (int)(clamp_val(x1, 0, model_in_w) / scale_w);
        group->results[last_count].box.top = (int)(clamp_val(y1, 0, model_in_h) / scale_h);
        group->results[last_count].box.right = (int)(clamp_val(x2, 0, model_in_w) / scale_w);
        group->results[last_count].box.bottom = (int)(clamp_val(y2, 0, model_in_h) / scale_h);
        group->results[last_count].prop = objProbs[n];  // 置信度

        // 复制类别名称
        if (id < class_num_ && labels_[id] != nullptr)
        {
            strncpy(group->results[last_count].name, labels_[id], OBJ_NAME_MAX_SIZE - 1);
            group->results[last_count].name[OBJ_NAME_MAX_SIZE - 1] = '\0';
        }

        last_count++;
    }
    group->count = last_count;
    return 0;
}

/* ============ 向后兼容的全局 API ============ */

// 全局后处理上下文（供旧 C API 使用）
static PostProcessContext g_global_ctx;

/**
 * 初始化标签路径（向后兼容）
 */
void initLabelPath(const char* model_path)
{
    g_global_ctx.init(model_path);
}

/**
 * 释放后处理资源（向后兼容）
 */
void deinitPostProcess()
{
    g_global_ctx.deinit();
}

/**
 * 后处理函数（向后兼容）
 */
int post_process(int8_t *input0, int8_t *input1, int8_t *input2,
                 int model_in_h, int model_in_w,
                 float conf_threshold, float nms_threshold,
                 BOX_RECT pads, float scale_w, float scale_h,
                 std::vector<int32_t> &qnt_zps, std::vector<float> &qnt_scales,
                 detect_result_group_t *group)
{
    return g_global_ctx.process(input0, input1, input2, model_in_h, model_in_w,
                                conf_threshold, nms_threshold, pads, scale_w, scale_h,
                                qnt_zps, qnt_scales, group);
}
