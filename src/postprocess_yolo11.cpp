/*
 * postprocess_yolo11.cpp - YOLO11 后处理模块实现
 *
 * 与 YOLOv5 后处理的区别：
 *   - YOLO11 是 anchor-free 模型，无预定义 anchor
 *   - 输出为单张量 [1, 8400, 84]（NHWC 布局）
 *   - 8400 = 80×80 + 40×40 + 20×20（三个 stride 级联）
 *   - 84 = 4 (cx,cy,w,h) + 80 类概率
 *   - 使用 DFL（Distribution Focal Loss）解码边框
 *
 * 后处理步骤：
 *   1. 量化反量化：int8 → float
 *   2. 遍历 8400 个预测点，解码边框和类别
 *   3. 置信度过滤
 *   4. NMS（按类别分组）
 *   5. 坐标还原到原图
 */

#include "postprocess_yolo11.hpp"
#include "config_loader.hpp"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

#include <set>
#include <vector>
#include <algorithm>
#include <numeric>

/* ============ 复用辅助函数 ============ */

/**
 * 限制值在 [min, max] 范围内
 */
static inline float clamp_val(float val, int min, int max)
{
    return val > min ? (val < max ? val : max) : min;
}

/**
 * Sigmoid 激活函数
 */
static inline float sigmoid_yolo(float x) { return 1.0f / (1.0f + expf(-x)); }

/**
 * 计算两个框的 IoU（Intersection over Union）
 */
static float CalculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0,
                              float xmin1, float ymin1, float xmax1, float ymax1)
{
    float w = fmax(0.f, fmin(xmax0, xmax1) - fmax(xmin0, xmin1) + 1.0);
    float h = fmax(0.f, fmin(ymax0, ymax1) - fmax(ymin0, ymin1) + 1.0);
    float i = w * h;  // 交集面积
    float u = (xmax0 - xmin0 + 1.0) * (ymax0 - ymin0 + 1.0) +
              (xmax1 - xmin1 + 1.0) * (ymax1 - ymin1 + 1.0) - i;
    return u <= 0.f ? 0.f : (i / u);  // IoU = 交集 / 并集
}

/**
 * NMS（非极大值抑制）
 *
 * 对同一类别的检测框，按置信度排序，抑制 IoU 高于阈值的框。
 *
 * @param validCount     有效检测框数量
 * @param outputLocations 输出坐标（x, y, w, h 格式）
 * @param classIds       各框的类别 ID
 * @param order          按置信度排序的索引数组（被抑制的设为 -1）
 * @param filterId       当前处理的类别 ID
 * @param threshold      IoU 阈值
 */
static int nms_yolo11(int validCount, std::vector<float> &outputLocations,
                      std::vector<int> classIds, std::vector<int> &order,
                      int filterId, float threshold)
{
    for (int i = 0; i < validCount; ++i) {
        if (order[i] == -1 || classIds[i] != filterId) continue;
        int n = order[i];

        for (int j = i + 1; j < validCount; ++j) {
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

            float iou = CalculateOverlap(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);
            if (iou > threshold) order[j] = -1;  // 抑制
        }
    }
    return 0;
}

/* ============ YOLO11PostProcess 实现 ============ */

/**
 * 构造函数：初始化成员变量
 */
YOLO11PostProcess::YOLO11PostProcess()
    : initialized_(false), class_num_(OBJ_CLASS_NUM)
{
    memset(label_path_, 0, sizeof(label_path_));
    memset(labels_, 0, sizeof(labels_));
}

/**
 * 析构函数：释放标签内存
 */
YOLO11PostProcess::~YOLO11PostProcess()
{
    deinit();
}

/**
 * 初始化后处理上下文
 *
 * 流程：
 *   1. 从配置加载类别数
 *   2. 从模型路径推导标签文件路径
 *   3. 读取标签文件
 *
 * @param model_path 模型文件路径
 * @return 0 成功，-1 失败
 */
int YOLO11PostProcess::init(const char* model_path)
{
    if (initialized_) return 0;  // 已初始化

    // 从配置加载类别数
    AppConfig cfg = load_config();
    class_num_ = cfg.class_num;

    // 从模型路径推导标签路径
    char* model_copy = strdup(model_path);
    char* dir = dirname(model_copy);
    snprintf(label_path_, sizeof(label_path_), "%s/../%s", dir, cfg.label_file.c_str());
    free(model_copy);

    // 加载标签文件（每行一个类别名）
    FILE* fp = fopen(label_path_, "r");
    if (!fp) {
        printf("[YOLO11] Warning: cannot open label file: %s\n", label_path_);
        return -1;
    }

    char line[512];
    int idx = 0;
    while (fgets(line, sizeof(line), fp) && idx < class_num_) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = 0;  // 去除换行符
        labels_[idx] = (char*)malloc(len + 1);
        if (labels_[idx]) {
            memcpy(labels_[idx], line, len + 1);
        }
        idx++;
    }
    fclose(fp);

    initialized_ = true;
    return 0;
}

/**
 * 释放标签内存
 */
void YOLO11PostProcess::deinit()
{
    if (!initialized_) return;
    for (int i = 0; i < class_num_; i++) {
        if (labels_[i]) {
            free(labels_[i]);
            labels_[i] = nullptr;
        }
    }
    initialized_ = false;
}

/* ============ 内部辅助 ============ */

/**
 * 量化反量化：int8 → float
 * 公式：float_value = (int8_value - zero_point) * scale
 */
static inline float deqnt_affine_to_f32(int8_t val, int32_t zp, float scale)
{
    return ((float)val - (float)zp) * scale;
}

/**
 * YOLO11 输出网格信息
 *
 * 三个 stride 的网格配置：
 *   - stride=8:  80×80 = 6400 个预测点（检测小目标）
 *   - stride=16: 40×40 = 1600 个预测点（检测中目标）
 *   - stride=32: 20×20 = 400 个预测点（检测大目标）
 *   - 总计：6400 + 1600 + 400 = 8400 个预测点
 */
struct GridInfo {
    int stride;     // 步长
    int count;      // 网格细胞数 (grid_h × grid_w)
    int offset;     // 在 8400 输出中的起始索引
};

static const GridInfo YOLO11_GRIDS[3] = {
    { 8,   80 * 80, 0 },                        // stride 8
    { 16,  40 * 40, 80 * 80 },                  // stride 16, offset=6400
    { 32,  20 * 20, 80 * 80 + 40 * 40 },        // stride 32, offset=8000
};

/**
 * YOLO11 后处理主函数
 *
 * 流程：
 *   1. 遍历 8400 个预测点
 *   2. 对每个点：解码边框 + 找最大类别概率
 *   3. 置信度过滤
 *   4. 按类别执行 NMS
 *   5. 坐标还原到原图
 *
 * @param output         NPU 输出张量（int8 量化）
 * @param model_h,model_w 模型输入尺寸
 * @param conf_threshold 置信度阈值
 * @param nms_threshold  NMS IoU 阈值
 * @param pads           letterbox 填充量
 * @param scale_w,scale_h 缩放比例
 * @param zp,scale       量化参数
 * @param group          输出检测结果
 */
int YOLO11PostProcess::process(int8_t *output, int model_h, int model_w,
                               float conf_threshold, float nms_threshold,
                               BOX_RECT pads, float scale_w, float scale_h,
                               int32_t zp, float scale,
                               detect_result_group_t *group)
{
    // 清零输出结构体
    memset(group, 0, sizeof(detect_result_group_t));

    const int TOTAL_CELLS = 8400;           // 总预测点数
    const int ATTRS_PER_CELL = 4 + class_num_;  // 每个点的属性数：4边框 + 类别概率

    // 预分配内存
    std::vector<float> boxes;
    std::vector<float> objProbs;
    std::vector<int> classId;

    boxes.reserve(TOTAL_CELLS * 4);
    objProbs.reserve(TOTAL_CELLS);
    classId.reserve(TOTAL_CELLS);

    // ========== 步骤1：遍历三个尺度的网格 ==========
    for (int g = 0; g < 3; g++) {
        int stride = YOLO11_GRIDS[g].stride;
        int count = YOLO11_GRIDS[g].count;
        int offset = YOLO11_GRIDS[g].offset;
        int grid_w = model_w / stride;
        int grid_h = model_h / stride;

        // 遍历该尺度的所有预测点
        for (int i = 0; i < count; i++) {
            int col = i % grid_w;  // 列号
            int row = i / grid_w;  // 行号
            int base = (offset + i) * ATTRS_PER_CELL;  // 该点在输出数组中的起始位置

            // ========== 步骤2：找最大类别概率 ==========
            int8_t max_cls_i8 = output[base + 4];  // 第一个类别
            int max_cls_id = 0;
            for (int k = 1; k < class_num_; k++) {
                int8_t prob = output[base + 4 + k];
                if (prob > max_cls_i8) {
                    max_cls_i8 = prob;
                    max_cls_id = k;
                }
            }

            // 反量化 + 置信度过滤
            float max_cls_prob = deqnt_affine_to_f32(max_cls_i8, zp, scale);
            if (max_cls_prob < conf_threshold) continue;

            // ========== 步骤3：解码边框（anchor-free） ==========
            float cx = deqnt_affine_to_f32(output[base + 0], zp, scale);
            float cy = deqnt_affine_to_f32(output[base + 1], zp, scale);
            float w  = deqnt_affine_to_f32(output[base + 2], zp, scale);
            float h  = deqnt_affine_to_f32(output[base + 3], zp, scale);

            // DFL 解码：sigmoid + 缩放
            cx = (sigmoid_yolo(cx) * 2.0f - 0.5f + (float)col) * (float)stride;
            cy = (sigmoid_yolo(cy) * 2.0f - 0.5f + (float)row) * (float)stride;
            w  = std::pow(sigmoid_yolo(w) * 2.0f, 2.0f) * (float)stride;
            h  = std::pow(sigmoid_yolo(h) * 2.0f, 2.0f) * (float)stride;

            // 转换为左上角坐标
            float x1 = cx - w / 2.0f;
            float y1 = cy - h / 2.0f;

            // 保存检测框
            boxes.push_back(x1);
            boxes.push_back(y1);
            boxes.push_back(w);
            boxes.push_back(h);
            objProbs.push_back(max_cls_prob);
            classId.push_back(max_cls_id);
        }
    }

    // ========== 步骤4：NMS ==========
    int validCount = (int)objProbs.size();
    if (validCount == 0) return 0;  // 无检测结果

    // 按置信度降序排序
    std::vector<int> indexArray(validCount);
    std::iota(indexArray.begin(), indexArray.end(), 0);
    std::sort(indexArray.begin(), indexArray.end(),
        [&objProbs](int a, int b) { return objProbs[a] > objProbs[b]; });

    // 按类别执行 NMS
    std::set<int> class_set(classId.begin(), classId.end());
    for (auto c : class_set) {
        nms_yolo11(validCount, boxes, classId, indexArray, c, nms_threshold);
    }

    // ========== 步骤5：输出结果 ==========
    int last_count = 0;
    for (int i = 0; i < validCount && last_count < OBJ_NUMB_MAX_SIZE; i++) {
        if (indexArray[i] == -1) continue;  // 已被 NMS 抑制
        int n = indexArray[i];

        // 坐标还原：减去 padding，除以缩放比例
        float x1 = boxes[n * 4 + 0] - (float)pads.left;
        float y1 = boxes[n * 4 + 1] - (float)pads.top;
        float x2 = x1 + boxes[n * 4 + 2];
        float y2 = y1 + boxes[n * 4 + 3];

        group->results[last_count].box.left   = clamp_val(x1 / scale_w, 0, model_w);
        group->results[last_count].box.top    = clamp_val(y1 / scale_h, 0, model_h);
        group->results[last_count].box.right  = clamp_val(x2 / scale_w, 0, model_w);
        group->results[last_count].box.bottom = clamp_val(y2 / scale_h, 0, model_h);
        group->results[last_count].prop       = objProbs[n];  // 置信度

        // 复制类别名称
        if (classId[n] < class_num_ && labels_[classId[n]]) {
            strncpy(group->results[last_count].name, labels_[classId[n]], OBJ_NAME_MAX_SIZE - 1);
            group->results[last_count].name[OBJ_NAME_MAX_SIZE - 1] = 0;
        }
        last_count++;
    }
    group->count = last_count;
    return 0;
}
