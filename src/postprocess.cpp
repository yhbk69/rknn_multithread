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

/* 全局配置（运行时参数） */
static AppConfig g_config;

/* 标签文件路径（运行时设置） */
static char g_label_path[512] = "";

/* Anchor 参数（从配置加载） */
static int g_anchor0[6];
static int g_anchor1[6];
static int g_anchor2[6];
static bool g_anchors_initialized = false;

static char *labels[OBJ_CLASS_NUM];

/**
 * 初始化标签路径和Anchor参数
 * @param model_path 模型文件路径，用于推导标签文件路径
 */
void initLabelPath(const char* model_path)
{
    if (g_anchors_initialized) return;

    /* 加载配置 */
    g_config = load_config();

    /* 从模型路径推导标签路径 */
    char* model_copy = strdup(model_path);
    char* dir = dirname(model_copy);
    snprintf(g_label_path, sizeof(g_label_path), "%s/../%s", dir, g_config.label_file.c_str());
    free(model_copy);

    /* 加载Anchor */
    memcpy(g_anchor0, g_config.anchor_small, sizeof(g_anchor0));
    memcpy(g_anchor1, g_config.anchor_medium, sizeof(g_anchor1));
    memcpy(g_anchor2, g_config.anchor_large, sizeof(g_anchor2));
    g_anchors_initialized = true;

    printf("Using label path: %s\n", g_label_path);
}

/**
 * 限制数值在[min, max]范围内
 */
inline static int clamp(float val, int min, int max) { return val > min ? (val < max ? val : max) : min; }

/**
 * 从文件中读取一行文本
 * @param fp 文件指针
 * @param buffer 输出缓冲区
 * @param len 输出行长度
 * @return 读取的行字符串
 */
char *readLine(FILE *fp, char *buffer, int *len)
{
  int ch;
  int i = 0;
  size_t buff_len = 0;

  buffer = (char *)malloc(buff_len + 1);
  if (!buffer)
    return NULL; // 内存不足

  // 逐字符读取直到换行或文件结束
  while ((ch = fgetc(fp)) != '\n' && ch != EOF)
  {
    buff_len++;
    void *tmp = realloc(buffer, buff_len + 1);
    if (tmp == NULL)
    {
      free(buffer);
      return NULL; // 内存不足
    }
    buffer = (char *)tmp;

    buffer[i] = (char)ch;
    i++;
  }
  buffer[i] = '\0';

  *len = buff_len;

  // 检测文件结束
  if (ch == EOF && (i == 0 || ferror(fp)))
  {
    free(buffer);
    return NULL;
  }
  return buffer;
}

/**
 * 读取文件的所有行
 * @param fileName 文件路径
 * @param lines 输出行数组
 * @param max_line 最大行数
 * @return 实际读取的行数
 */
int readLines(const char *fileName, char *lines[], int max_line)
{
  FILE *file = fopen(fileName, "r");
  char *s;
  int i = 0;
  int n = 0;

  if (file == NULL)
  {
    printf("Open %s fail!\n", fileName);
    return -1;
  }

  while ((s = readLine(file, s, &n)) != NULL)
  {
    lines[i++] = s;
    if (i >= max_line)
      break;
  }
  fclose(file);
  return i;
}

/**
 * 加载类别标签名称
 * @param locationFilename 标签文件路径
 * @param label 输出标签数组
 * @return 0成功，-1失败
 */
int loadLabelName(const char *locationFilename, char *label[])
{
  printf("loadLabelName %s\n", locationFilename);
  readLines(locationFilename, label, OBJ_CLASS_NUM);
  return 0;
}

/**
 * 计算两个边界框的IoU(交并比)
 * IoU = 交集面积 / 并集面积
 * 用于NMS判断两个框是否重叠度过高
 */
static float CalculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0, float xmin1, float ymin1, float xmax1,
                              float ymax1)
{
  // 计算交集区域的宽高
  float w = fmax(0.f, fmin(xmax0, xmax1) - fmax(xmin0, xmin1) + 1.0);
  float h = fmax(0.f, fmin(ymax0, ymax1) - fmax(ymin0, ymin1) + 1.0);
  float i = w * h; // 交集面积

  // 并集面积 = 框1面积 + 框2面积 - 交集面积
  float u = (xmax0 - xmin0 + 1.0) * (ymax0 - ymin0 + 1.0) + (xmax1 - xmin1 + 1.0) * (ymax1 - ymin1 + 1.0) - i;
  return u <= 0.f ? 0.f : (i / u);
}

/**
 * NMS(非极大值抑制): 去除重叠度高的冗余检测框
 * 对于同一类别的多个重叠框，只保留置信度最高的那个
 *
 * @param validCount 有效检测框数量
 * @param outputLocations 所有检测框的坐标[x, y, w, h]
 * @param classIds 每个框对应的类别ID
 * @param order 按置信度排序的索引数组
 * @param filterId 当前要处理的类别ID
 * @param threshold IoU阈值，超过此值认为是重复框
 * @return 0成功
 */
static int nms(int validCount, std::vector<float> &outputLocations, std::vector<int> classIds, std::vector<int> &order,
               int filterId, float threshold)
{
  for (int i = 0; i < validCount; ++i)
  {
    // 跳过已标记为重复或非当前类别的框
    if (order[i] == -1 || classIds[i] != filterId)
    {
      continue;
    }
    int n = order[i];
    for (int j = i + 1; j < validCount; ++j)
    {
      int m = order[j];
      if (m == -1 || classIds[i] != filterId)
      {
        continue;
      }
      // 获取两个框的坐标
      float xmin0 = outputLocations[n * 4 + 0];
      float ymin0 = outputLocations[n * 4 + 1];
      float xmax0 = outputLocations[n * 4 + 0] + outputLocations[n * 4 + 2];
      float ymax0 = outputLocations[n * 4 + 1] + outputLocations[n * 4 + 3];

      float xmin1 = outputLocations[m * 4 + 0];
      float ymin1 = outputLocations[m * 4 + 1];
      float xmax1 = outputLocations[m * 4 + 0] + outputLocations[m * 4 + 2];
      float ymax1 = outputLocations[m * 4 + 1] + outputLocations[m * 4 + 3];

      // 计算IoU，如果超过阈值则标记为重复(置为-1)
      float iou = CalculateOverlap(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);

      if (iou > threshold)
      {
        order[j] = -1;
      }
    }
  }
  return 0;
}

/**
 * 快速排序(降序)，同时维护索引映射
 * 用于按置信度对检测框进行排序
 *
 * @param input 置信度数组
 * @param left 左边界
 * @param right 右边界
 * @param indices 索引数组(会随排序同步调整)
 * @return 基准元素的最终位置
 */
static int quick_sort_indice_inverse(std::vector<float> &input, int left, int right, std::vector<int> &indices)
{
  float key;
  int key_index;
  int low = left;
  int high = right;
  if (left < right)
  {
    key_index = indices[left];
    key = input[left];
    while (low < high)
    {
      // 从右向左找第一个大于key的元素
      while (low < high && input[high] <= key)
      {
        high--;
      }
      input[low] = input[high];
      indices[low] = indices[high];
      // 从左向右找第一个小于等于key的元素
      while (low < high && input[low] >= key)
      {
        low++;
      }
      input[high] = input[low];
      indices[high] = indices[low];
    }
    input[low] = key;
    indices[low] = key_index;
    // 递归排序左右子数组
    quick_sort_indice_inverse(input, left, low - 1, indices);
    quick_sort_indice_inverse(input, low + 1, right, indices);
  }
  return low;
}

/**
 * Sigmoid激活函数: 将任意实数映射到(0, 1)区间
 * 用于将模型输出转换为概率/置信度
 */
static float sigmoid(float x) { return 1.0 / (1.0 + expf(-x)); }

/**
 * Sigmoid的反函数: 将概率值转换回logit空间
 * 用于ReLU模型的后处理(因为ReLU模型跳过了sigmoid)
 */
static float unsigmoid(float y) { return -1.0 * logf((1.0 / y) - 1.0); }

/**
 * 限制浮点数在[min, max]范围内，并转换为整数
 */
inline static int32_t __clip(float val, float min, float max)
{
  float f = val <= min ? min : (val >= max ? max : val);
  return f;
}

/**
 * 浮点数转INT8量化值
 * @param f32 浮点数值
 * @param zp 零点(zero point)
 * @param scale 缩放因子
 * @return INT8量化值(-128 ~ 127)
 */
static int8_t qnt_f32_to_affine(float f32, int32_t zp, float scale)
{
  float dst_val = (f32 / scale) + zp;
  int8_t res = (int8_t)__clip(dst_val, -128, 127);
  return res;
}

/**
 * INT8量化值转浮点数(反量化)
 * @param qnt INT8量化值
 * @param zp 零点
 * @param scale 缩放因子
 * @return 浮点数值
 */
static float deqnt_affine_to_f32(int8_t qnt, int32_t zp, float scale) { return ((float)qnt - (float)zp) * scale; }

/**
 * 处理单个尺度的特征图，解码检测框
 * YOLOv5输出3个尺度的特征图，分别检测大、中、小目标
 *
 * @param input 特征图数据(INT8量化)
 * @param anchor 当前尺度的Anchor参数(3组)
 * @param grid_h 特征图高度
 * @param grid_w 特征图宽度
 * @param height 模型输入高度
 * @param width 模型输入宽度
 * @param stride 下采样倍率(8/16/32)
 * @param boxes 输出的检测框坐标
 * @param objProbs 输出的目标置信度
 * @param classId 输出的类别ID
 * @param threshold 置信度阈值
 * @param zp 量化零点
 * @param scale 量化缩放因子
 * @return 有效检测框数量
 */
static int process(int8_t *input, int *anchor, int grid_h, int grid_w, int height, int width, int stride,
                   std::vector<float> &boxes, std::vector<float> &objProbs, std::vector<int> &classId, float threshold,
                   int32_t zp, float scale)
{
  int validCount = 0;
  int grid_len = grid_h * grid_w; // 特征图总格子数

  // 将浮点阈值转换为INT8量化阈值，避免在循环中重复转换
  int8_t thres_i8 = qnt_f32_to_affine(threshold, zp, scale);

  // 遍历3个Anchor
  for (int a = 0; a < 3; a++)
  {
    // 遍历特征图的每个格子
    for (int i = 0; i < grid_h; i++)
    {
      for (int j = 0; j < grid_w; j++)
      {
        // 获取目标置信度(box confidence)
        // PROP_BOX_SIZE=85 = 4(坐标) + 1(置信度) + 80(类别)
        int8_t box_confidence = input[(PROP_BOX_SIZE * a + 4) * grid_len + i * grid_w + j];

        // 如果目标置信度低于阈值，跳过该格子
        if (box_confidence >= thres_i8)
        {
          int offset = (PROP_BOX_SIZE * a) * grid_len + i * grid_w + j;
          int8_t *in_ptr = input + offset;

          // 解码边界框坐标
          // YOLOv5使用新的解码方式: (sigmoid(x)*2-0.5+grid)*stride
          // 这里因为模型用了ReLU替换sigmoid，所以直接计算
          float box_x = (deqnt_affine_to_f32(*in_ptr, zp, scale)) * 2.0 - 0.5;
          float box_y = (deqnt_affine_to_f32(in_ptr[grid_len], zp, scale)) * 2.0 - 0.5;
          float box_w = (deqnt_affine_to_f32(in_ptr[2 * grid_len], zp, scale)) * 2.0;
          float box_h = (deqnt_affine_to_f32(in_ptr[3 * grid_len], zp, scale)) * 2.0;

          // 将相对坐标转换为绝对像素坐标
          box_x = (box_x + j) * (float)stride;
          box_y = (box_y + i) * (float)stride;

          // 将相对宽高转换为绝对像素宽高
          box_w = box_w * box_w * (float)anchor[a * 2];
          box_h = box_h * box_h * (float)anchor[a * 2 + 1];

          // 转换为中心点坐标到左上角坐标
          box_x -= (box_w / 2.0);
          box_y -= (box_h / 2.0);

          // 找出80个类别中概率最高的类别
          int8_t maxClassProbs = in_ptr[5 * grid_len];
          int maxClassId = 0;
          for (int k = 1; k < g_config.class_num; ++k)
          {
            int8_t prob = in_ptr[(5 + k) * grid_len];
            if (prob > maxClassProbs)
            {
              maxClassId = k;
              maxClassProbs = prob;
            }
          }

          // 如果最高类别的概率也超过阈值，则保留该检测框
          if (maxClassProbs > thres_i8)
          {
            // 最终置信度 = 目标置信度 * 类别置信度
            objProbs.push_back((deqnt_affine_to_f32(maxClassProbs, zp, scale)) * (deqnt_affine_to_f32(box_confidence, zp, scale)));
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

/**
 * YOLOv5后处理主函数
 * 对3个尺度的输出进行解码、置信度过滤、NMS去重
 *
 * @param input0 第1个尺度(8倍下采样)的输出
 * @param input1 第2个尺度(16倍下采样)的输出
 * @param input2 第3个尺度(32倍下采样)的输出
 * @param model_in_h 模型输入高度(640)
 * @param model_in_w 模型输入宽度(640)
 * @param conf_threshold 置信度阈值
 * @param nms_threshold NMS的IoU阈值
 * @param pads letterbox填充信息
 * @param scale_w 宽度缩放比例
 * @param scale_h 高度缩放比例
 * @param qnt_zps 各输出的量化零点
 * @param qnt_scales 各输出的量化缩放因子
 * @param group 输出的检测结果组
 * @return 0成功，-1失败
 */
int post_process(int8_t *input0, int8_t *input1, int8_t *input2, int model_in_h, int model_in_w, float conf_threshold,
                 float nms_threshold, BOX_RECT pads, float scale_w, float scale_h, std::vector<int32_t> &qnt_zps,
                 std::vector<float> &qnt_scales, detect_result_group_t *group)
{
    /* 只在第一次调用时初始化 */
    static int init = -1;
    if (init == -1)
    {
        int ret = loadLabelName(g_label_path, labels);
        if (ret < 0)
        {
            return -1;
        }
        init = 0;
    }

  memset(group, 0, sizeof(detect_result_group_t));

  std::vector<float> filterBoxes;  // 所有检测框坐标
  std::vector<float> objProbs;     // 所有检测框的置信度
  std::vector<int> classId;        // 所有检测框的类别ID

  // ====== 处理第1个尺度: 8倍下采样，检测小目标 ======
  int stride0 = 8;
  int grid_h0 = model_in_h / stride0;  // 640/8 = 80
  int grid_w0 = model_in_w / stride0;  // 640/8 = 80
  int validCount0 = 0;
  validCount0 = process(input0, g_anchor0, grid_h0, grid_w0, model_in_h, model_in_w, stride0, filterBoxes, objProbs,
                        classId, conf_threshold, qnt_zps[0], qnt_scales[0]);

  // ====== 处理第2个尺度: 16倍下采样，检测中等目标 ======
  int stride1 = 16;
  int grid_h1 = model_in_h / stride1;  // 640/16 = 40
  int grid_w1 = model_in_w / stride1;  // 640/16 = 40
  int validCount1 = 0;
  validCount1 = process(input1, g_anchor1, grid_h1, grid_w1, model_in_h, model_in_w, stride1, filterBoxes, objProbs,
                        classId, conf_threshold, qnt_zps[1], qnt_scales[1]);

  // ====== 处理第3个尺度: 32倍下采样，检测大目标 ======
  int stride2 = 32;
  int grid_h2 = model_in_h / stride2;  // 640/32 = 20
  int grid_w2 = model_in_w / stride2;  // 640/32 = 20
  int validCount2 = 0;
  validCount2 = process(input2, g_anchor2, grid_h2, grid_w2, model_in_h, model_in_w, stride2, filterBoxes, objProbs,
                        classId, conf_threshold, qnt_zps[2], qnt_scales[2]);

  // 汇总3个尺度的有效检测框数量
  int validCount = validCount0 + validCount1 + validCount2;

  // 没有检测到任何目标
  if (validCount <= 0)
  {
    return 0;
  }

  // 创建索引数组，用于排序
  std::vector<int> indexArray;
  for (int i = 0; i < validCount; ++i)
  {
    indexArray.push_back(i);
  }

  // 按置信度降序排序
  quick_sort_indice_inverse(objProbs, 0, validCount - 1, indexArray);

  // 获取所有出现过的类别
  std::set<int> class_set(std::begin(classId), std::end(classId));

  // 对每个类别分别执行NMS
  for (auto c : class_set)
  {
    nms(validCount, filterBoxes, classId, indexArray, c, nms_threshold);
  }

  // 收集最终的检测结果
  int last_count = 0;
  group->count = 0;

  for (int i = 0; i < validCount; ++i)
  {
    // 跳过被NMS过滤掉的框
    if (indexArray[i] == -1 || last_count >= OBJ_NUMB_MAX_SIZE)
    {
      continue;
    }
    int n = indexArray[i];

    // 将坐标从模型尺寸映射回原始图像尺寸
    // 减去letterbox的填充，再除以缩放比例
    float x1 = filterBoxes[n * 4 + 0] - pads.left;
    float y1 = filterBoxes[n * 4 + 1] - pads.top;
    float x2 = x1 + filterBoxes[n * 4 + 2];
    float y2 = y1 + filterBoxes[n * 4 + 3];
    int id = classId[n];
    float obj_conf = objProbs[i];

    // 限制坐标在图像范围内，并转换回原始图像尺寸
    group->results[last_count].box.left = (int)(clamp(x1, 0, model_in_w) / scale_w);
    group->results[last_count].box.top = (int)(clamp(y1, 0, model_in_h) / scale_h);
    group->results[last_count].box.right = (int)(clamp(x2, 0, model_in_w) / scale_w);
    group->results[last_count].box.bottom = (int)(clamp(y2, 0, model_in_h) / scale_h);
    group->results[last_count].prop = obj_conf;

    // 复制类别名称
    char *label = labels[id];
    strncpy(group->results[last_count].name, label, OBJ_NAME_MAX_SIZE);

    last_count++;
  }
  group->count = last_count;

  return 0;
}

/**
 * 释放后处理分配的内存(类别标签)
 */
void deinitPostProcess()
{
  for (int i = 0; i < g_config.class_num; i++)
  {
    if (labels[i] != nullptr)
    {
      free(labels[i]);
      labels[i] = nullptr;
    }
  }
}
