// Copyright (c) 2021 by Rockchip Electronics Co., Ltd. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

/* ============ PostProcessContext 实现 ============ */

PostProcessContext::PostProcessContext()
    : initialized_(false), class_num_(OBJ_CLASS_NUM)
{
    memset(label_path_, 0, sizeof(label_path_));
    memset(anchor_small_, 0, sizeof(anchor_small_));
    memset(anchor_medium_, 0, sizeof(anchor_medium_));
    memset(anchor_large_, 0, sizeof(anchor_large_));
    memset(labels_, 0, sizeof(labels_));
}

PostProcessContext::~PostProcessContext()
{
    deinit();
}

int PostProcessContext::init(const char* model_path)
{
    if (initialized_) return 0;

    AppConfig cfg = load_config();
    class_num_ = cfg.class_num;

    /* 从模型路径推导标签路径 */
    char* model_copy = strdup(model_path);
    char* dir = dirname(model_copy);
    snprintf(label_path_, sizeof(label_path_), "%s/../%s", dir, cfg.label_file.c_str());
    free(model_copy);

    /* 加载 Anchor */
    memcpy(anchor_small_, cfg.anchor_small, sizeof(anchor_small_));
    memcpy(anchor_medium_, cfg.anchor_medium, sizeof(anchor_medium_));
    memcpy(anchor_large_, cfg.anchor_large, sizeof(anchor_large_));

    /* 加载标签 */
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

        int len = 0;
        while ((ch = fgetc(file)) != '\n' && ch != EOF)
        {
            if (len + 1 >= (int)buf_cap)
            {
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

static inline int clamp_val(float val, int min, int max)
{
    return val > min ? (val < max ? val : max) : min;
}

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

static int nms(int validCount, std::vector<float> &outputLocations,
               std::vector<int> classIds, std::vector<int> &order,
               int filterId, float threshold)
{
    for (int i = 0; i < validCount; ++i)
    {
        if (order[i] == -1 || classIds[i] != filterId) continue;
        int n = order[i];
        for (int j = i + 1; j < validCount; ++j)
        {
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

static void quick_sort_indice_inverse(std::vector<float> &input, int left, int right, std::vector<int> &indices)
{
    if (left >= right) return;
    float key = input[left];
    int key_index = indices[left];
    int low = left, high = right;
    while (low < high)
    {
        while (low < high && input[high] <= key) high--;
        input[low] = input[high];
        indices[low] = indices[high];
        while (low < high && input[low] >= key) low++;
        input[high] = input[low];
        indices[high] = indices[low];
    }
    input[low] = key;
    indices[low] = key_index;
    quick_sort_indice_inverse(input, left, low - 1, indices);
    quick_sort_indice_inverse(input, low + 1, right, indices);
}

static inline float sigmoid(float x) { return 1.0f / (1.0f + expf(-x)); }

static inline int32_t clip_float(float val, float min, float max)
{
    float f = val <= min ? min : (val >= max ? max : val);
    return f;
}

static int8_t qnt_f32_to_affine(float f32, int32_t zp, float scale)
{
    float dst_val = (f32 / scale) + zp;
    return (int8_t)clip_float(dst_val, -128, 127);
}

static float deqnt_affine_to_f32(int8_t qnt, int32_t zp, float scale)
{
    return ((float)qnt - (float)zp) * scale;
}

static int process_single_scale(int8_t *input, int *anchor, int grid_h, int grid_w,
                                int height, int width, int stride, int class_num,
                                std::vector<float> &boxes, std::vector<float> &objProbs,
                                std::vector<int> &classId, float threshold,
                                int32_t zp, float scale)
{
    int validCount = 0;
    int grid_len = grid_h * grid_w;
    int8_t thres_i8 = qnt_f32_to_affine(threshold, zp, scale);

    for (int a = 0; a < 3; a++)
    {
        for (int i = 0; i < grid_h; i++)
        {
            for (int j = 0; j < grid_w; j++)
            {
                int8_t box_confidence = input[(PROP_BOX_SIZE * a + 4) * grid_len + i * grid_w + j];
                if (box_confidence >= thres_i8)
                {
                    int offset = (PROP_BOX_SIZE * a) * grid_len + i * grid_w + j;
                    int8_t *in_ptr = input + offset;

                    float box_x = (deqnt_affine_to_f32(*in_ptr, zp, scale)) * 2.0f - 0.5f;
                    float box_y = (deqnt_affine_to_f32(in_ptr[grid_len], zp, scale)) * 2.0f - 0.5f;
                    float box_w = (deqnt_affine_to_f32(in_ptr[2 * grid_len], zp, scale)) * 2.0f;
                    float box_h = (deqnt_affine_to_f32(in_ptr[3 * grid_len], zp, scale)) * 2.0f;

                    box_x = (box_x + j) * (float)stride;
                    box_y = (box_y + i) * (float)stride;
                    box_w = box_w * box_w * (float)anchor[a * 2];
                    box_h = box_h * box_h * (float)anchor[a * 2 + 1];
                    box_x -= (box_w / 2.0f);
                    box_y -= (box_h / 2.0f);

                    int8_t maxClassProbs = in_ptr[5 * grid_len];
                    int maxClassId = 0;
                    for (int k = 1; k < class_num; ++k)
                    {
                        int8_t prob = in_ptr[(5 + k) * grid_len];
                        if (prob > maxClassProbs)
                        {
                            maxClassId = k;
                            maxClassProbs = prob;
                        }
                    }

                    if (maxClassProbs > thres_i8)
                    {
                        objProbs.push_back(
                            (deqnt_affine_to_f32(maxClassProbs, zp, scale)) *
                            (deqnt_affine_to_f32(box_confidence, zp, scale)));
                        classId.push_back(maxClassId);
                        validCount++;
                        boxes.push_back(box_x);
                        boxes.push_back(box_y);
                        boxes.push_back(box_w);
                        boxes.push_back(box_h);
                    }
                }
            }
        }
    }
    return validCount;
}

/* ============ PostProcessContext::process ============ */

int PostProcessContext::process(int8_t *input0, int8_t *input1, int8_t *input2,
                                int model_in_h, int model_in_w,
                                float conf_threshold, float nms_threshold,
                                BOX_RECT pads, float scale_w, float scale_h,
                                std::vector<int32_t> &qnt_zps, std::vector<float> &qnt_scales,
                                detect_result_group_t *group)
{
    memset(group, 0, sizeof(detect_result_group_t));

    std::vector<float> filterBoxes;
    std::vector<float> objProbs;
    std::vector<int> classId;

    int stride0 = 8;
    int validCount0 = process_single_scale(input0, anchor_small_,
        model_in_h / stride0, model_in_w / stride0, model_in_h, model_in_w, stride0,
        class_num_, filterBoxes, objProbs, classId, conf_threshold, qnt_zps[0], qnt_scales[0]);

    int stride1 = 16;
    int validCount1 = process_single_scale(input1, anchor_medium_,
        model_in_h / stride1, model_in_w / stride1, model_in_h, model_in_w, stride1,
        class_num_, filterBoxes, objProbs, classId, conf_threshold, qnt_zps[1], qnt_scales[1]);

    int stride2 = 32;
    int validCount2 = process_single_scale(input2, anchor_large_,
        model_in_h / stride2, model_in_w / stride2, model_in_h, model_in_w, stride2,
        class_num_, filterBoxes, objProbs, classId, conf_threshold, qnt_zps[2], qnt_scales[2]);

    int validCount = validCount0 + validCount1 + validCount2;
    if (validCount <= 0) return 0;

    std::vector<int> indexArray;
    for (int i = 0; i < validCount; ++i)
        indexArray.push_back(i);

    quick_sort_indice_inverse(objProbs, 0, validCount - 1, indexArray);

    std::set<int> class_set(std::begin(classId), std::end(classId));
    for (auto c : class_set)
        nms(validCount, filterBoxes, classId, indexArray, c, nms_threshold);

    int last_count = 0;
    group->count = 0;

    for (int i = 0; i < validCount; ++i)
    {
        if (indexArray[i] == -1 || last_count >= OBJ_NUMB_MAX_SIZE) continue;
        int n = indexArray[i];

        float x1 = filterBoxes[n * 4 + 0] - pads.left;
        float y1 = filterBoxes[n * 4 + 1] - pads.top;
        float x2 = x1 + filterBoxes[n * 4 + 2];
        float y2 = y1 + filterBoxes[n * 4 + 3];
        int id = classId[n];

        group->results[last_count].box.left = (int)(clamp_val(x1, 0, model_in_w) / scale_w);
        group->results[last_count].box.top = (int)(clamp_val(y1, 0, model_in_h) / scale_h);
        group->results[last_count].box.right = (int)(clamp_val(x2, 0, model_in_w) / scale_w);
        group->results[last_count].box.bottom = (int)(clamp_val(y2, 0, model_in_h) / scale_h);
        group->results[last_count].prop = objProbs[i];

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

static PostProcessContext g_global_ctx;

void initLabelPath(const char* model_path)
{
    g_global_ctx.init(model_path);
}

void deinitPostProcess()
{
    g_global_ctx.deinit();
}

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
