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

/* ============ 复用辅助函数（与 postprocess.cpp 共享） ============ */

static inline float clamp_val(float val, int min, int max)
{
    return val > min ? (val < max ? val : max) : min;
}

static inline float sigmoid_yolo(float x) { return 1.0f / (1.0f + expf(-x)); }

static float CalculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0,
                              float xmin1, float ymin1, float xmax1, float ymax1)
{
    float w = fmax(0.f, fmin(xmax0, xmax1) - fmax(xmin0, xmin1) + 1.0);
    float h = fmax(0.f, fmin(ymax0, ymax1) - fmax(ymin0, ymin1) + 1.0);
    float i = w * h;
    float u = (xmax0 - xmin0 + 1.0) * (ymax0 - ymin0 + 1.0) +
              (xmax1 - xmin1 + 1.0) * (ymax1 - ymin1 + 1.0) - i;
    return u <= 0.f ? 0.f : (i / u);
}

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

            float xmin0 = outputLocations[n * 4 + 0];
            float ymin0 = outputLocations[n * 4 + 1];
            float xmax0 = outputLocations[n * 4 + 0] + outputLocations[n * 4 + 2];
            float ymax0 = outputLocations[n * 4 + 1] + outputLocations[n * 4 + 3];

            float xmin1 = outputLocations[m * 4 + 0];
            float ymin1 = outputLocations[m * 4 + 1];
            float xmax1 = outputLocations[m * 4 + 0] + outputLocations[m * 4 + 2];
            float ymax1 = outputLocations[m * 4 + 1] + outputLocations[m * 4 + 3];

            float iou = CalculateOverlap(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);
            if (iou > threshold) order[j] = -1;
        }
    }
    return 0;
}

/* ============ YOLO11PostProcess 实现 ============ */

YOLO11PostProcess::YOLO11PostProcess()
    : initialized_(false), class_num_(OBJ_CLASS_NUM)
{
    memset(label_path_, 0, sizeof(label_path_));
    memset(labels_, 0, sizeof(labels_));
}

YOLO11PostProcess::~YOLO11PostProcess()
{
    deinit();
}

int YOLO11PostProcess::init(const char* model_path)
{
    if (initialized_) return 0;

    AppConfig cfg = load_config();
    class_num_ = cfg.class_num;

    /* 从模型路径推导标签路径 */
    char* model_copy = strdup(model_path);
    char* dir = dirname(model_copy);
    snprintf(label_path_, sizeof(label_path_), "%s/../%s", dir, cfg.label_file.c_str());
    free(model_copy);

    /* 加载标签文件 */
    FILE* fp = fopen(label_path_, "r");
    if (!fp) {
        printf("[YOLO11] Warning: cannot open label file: %s\n", label_path_);
        return -1;
    }

    char line[512];
    int idx = 0;
    while (fgets(line, sizeof(line), fp) && idx < class_num_) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = 0;
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

static inline float deqnt_affine_to_f32(int8_t val, int32_t zp, float scale)
{
    return ((float)val - (float)zp) * scale;
}

/* YOLO11 输出网格信息：三个 stride（8/16/32）的细胞数 */
struct GridInfo {
    int stride;
    int count;   // grid_h * grid_w
    int offset;  // 在 8400 输出中的起始索引
};

static const GridInfo YOLO11_GRIDS[3] = {
    { 8,   80 * 80, 0 },           // stride 8
    { 16,  40 * 40, 80 * 80 },     // stride 16, offset=6400
    { 32,  20 * 20, 80 * 80 + 40 * 40 },  // stride 32, offset=8000
};

int YOLO11PostProcess::process(int8_t *output, int model_h, int model_w,
                               float conf_threshold, float nms_threshold,
                               BOX_RECT pads, float scale_w, float scale_h,
                               int32_t zp, float scale,
                               detect_result_group_t *group)
{
    memset(group, 0, sizeof(detect_result_group_t));

    const int TOTAL_CELLS = 8400;
    const int ATTRS_PER_CELL = 4 + class_num_;  // cx,cy,w,h + cls probs

    std::vector<float> boxes;
    std::vector<float> objProbs;
    std::vector<int> classId;

    boxes.reserve(TOTAL_CELLS * 4);
    objProbs.reserve(TOTAL_CELLS);
    classId.reserve(TOTAL_CELLS);

    for (int g = 0; g < 3; g++) {
        int stride = YOLO11_GRIDS[g].stride;
        int count = YOLO11_GRIDS[g].count;
        int offset = YOLO11_GRIDS[g].offset;
        int grid_w = model_w / stride;
        int grid_h = model_h / stride;

        for (int i = 0; i < count; i++) {
            int col = i % grid_w;
            int row = i / grid_w;
            int base = (offset + i) * ATTRS_PER_CELL;

            /* 类别概率：取最大值 */
            int8_t max_cls_i8 = output[base + 4];
            int max_cls_id = 0;
            for (int k = 1; k < class_num_; k++) {
                int8_t prob = output[base + 4 + k];
                if (prob > max_cls_i8) {
                    max_cls_i8 = prob;
                    max_cls_id = k;
                }
            }

            float max_cls_prob = deqnt_affine_to_f32(max_cls_i8, zp, scale);
            if (max_cls_prob < conf_threshold) continue;

            /* 解码边框（anchor-free，基于 grid 坐标） */
            float cx = deqnt_affine_to_f32(output[base + 0], zp, scale);
            float cy = deqnt_affine_to_f32(output[base + 1], zp, scale);
            float w  = deqnt_affine_to_f32(output[base + 2], zp, scale);
            float h  = deqnt_affine_to_f32(output[base + 3], zp, scale);

            cx = (sigmoid_yolo(cx) * 2.0f - 0.5f + (float)col) * (float)stride;
            cy = (sigmoid_yolo(cy) * 2.0f - 0.5f + (float)row) * (float)stride;
            w  = std::pow(sigmoid_yolo(w) * 2.0f, 2.0f) * (float)stride;
            h  = std::pow(sigmoid_yolo(h) * 2.0f, 2.0f) * (float)stride;

            float x1 = cx - w / 2.0f;
            float y1 = cy - h / 2.0f;

            boxes.push_back(x1);
            boxes.push_back(y1);
            boxes.push_back(w);
            boxes.push_back(h);
            objProbs.push_back(max_cls_prob);
            classId.push_back(max_cls_id);
        }
    }

    int validCount = (int)objProbs.size();
    if (validCount == 0) return 0;

    /* NMS */
    std::vector<int> indexArray(validCount);
    std::iota(indexArray.begin(), indexArray.end(), 0);
    std::sort(indexArray.begin(), indexArray.end(),
        [&objProbs](int a, int b) { return objProbs[a] > objProbs[b]; });

    std::set<int> class_set(classId.begin(), classId.end());
    for (auto c : class_set) {
        nms_yolo11(validCount, boxes, classId, indexArray, c, nms_threshold);
    }

    /* 输出结果 */
    int last_count = 0;
    for (int i = 0; i < validCount && last_count < OBJ_NUMB_MAX_SIZE; i++) {
        if (indexArray[i] == -1) continue;
        int n = indexArray[i];

        float x1 = boxes[n * 4 + 0] - (float)pads.left;
        float y1 = boxes[n * 4 + 1] - (float)pads.top;
        float x2 = x1 + boxes[n * 4 + 2];
        float y2 = y1 + boxes[n * 4 + 3];

        group->results[last_count].box.left   = clamp_val(x1 / scale_w, 0, model_w);
        group->results[last_count].box.top    = clamp_val(y1 / scale_h, 0, model_h);
        group->results[last_count].box.right  = clamp_val(x2 / scale_w, 0, model_w);
        group->results[last_count].box.bottom = clamp_val(y2 / scale_h, 0, model_h);
        group->results[last_count].prop       = objProbs[n];

        if (classId[n] < class_num_ && labels_[classId[n]]) {
            strncpy(group->results[last_count].name, labels_[classId[n]], OBJ_NAME_MAX_SIZE - 1);
            group->results[last_count].name[OBJ_NAME_MAX_SIZE - 1] = 0;
        }
        last_count++;
    }
    group->count = last_count;
    return 0;
}
