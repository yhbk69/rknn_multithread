/*
 * Copyright (C) 2016 Rockchip Electronics Co., Ltd.
 * Authors:
 *  Zhiqin Wei <wzq@rock-chips.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _rga_utils_h_
#define _rga_utils_h_

// -------------------------------------------------------------------------------
// 以下为 RGA 调试与辅助工具函数
//
/* 根据像素格式获取每像素位数（bits per pixel） */
float get_bpp_from_format(int format);
/* 根据像素格式获取每像素步长（bytes per pixel） */
int get_perPixel_stride_from_format(int format);
/* 从文件中读取图像数据到缓冲区（用于输入测试） */
int get_buf_from_file(void *buf, int f, int sw, int sh, int index);
/* 将缓冲区数据输出到文件（用于调试验证） */
int output_buf_data_to_file(void *buf, int f, int sw, int sh, int index);
/* 将 RGA 像素格式枚举值转换为可读字符串 */
const char *translate_format_str(int format);
/* 从文件读取 FBC（帧缓冲压缩）格式数据到缓冲区 */
int get_buf_from_file_FBC(void *buf, int f, int sw, int sh, int index);
/* 将缓冲区数据以 FBC 格式输出到文件 */
int output_buf_data_to_file_FBC(void *buf, int f, int sw, int sh, int index);
#endif

