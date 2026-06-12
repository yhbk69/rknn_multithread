/*
 * Copyright (C) 2022 Rockchip Electronics Co., Ltd.
 * Authors:
 *  Cerf Yu <cerf.yu@rock-chips.com>
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
#ifndef _im2d_task_h_
#define _im2d_task_h_

#include "im2d_type.h"

#ifdef __cplusplus

/**
 * 创建 RGA job
 *
 * @param flags
 *      job 配置标志位
 *
 * @returns job 句柄
 */
IM_API im_job_handle_t imbeginJob(uint64_t flags = 0);

/**
 * 提交并运行 RGA job
 *
 * @param job_handle
 *      要提交的 job 句柄
 * @param sync_mode
 *      运行模式:
 *          IM_SYNC   - 同步模式，等待完成
 *          IM_ASYNC  - 异步模式，立即返回
 * @param acquire_fence_fd   输入同步 fence fd
 * @param release_fence_fd   输出同步 fence fd
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imendJob(im_job_handle_t job_handle,
                          int sync_mode = IM_SYNC,
                          int acquire_fence_fd = 0, int *release_fence_fd = NULL);

/**
 * 取消并删除 RGA job
 *
 * @param job_handle
 *      要取消的 job 句柄
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imcancelJob(im_job_handle_t job_handle);

/**
 * 添加复制任务
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param src         输入源图像
 * @param dst         输出目标图像
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imcopyTask(im_job_handle_t job_handle, const rga_buffer_t src, rga_buffer_t dst);

/**
 * 添加缩放任务
 *
 * @param job_handle     将任务插入到此 job 句柄中
 * @param src            输入源图像
 * @param dst            输出目标图像
 * @param fx             X 方向缩放因子
 * @param fy             Y 方向缩放因子
 * @param interpolation  插值算法（仅 RGA1 支持）
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imresizeTask(im_job_handle_t job_handle,
                              const rga_buffer_t src, rga_buffer_t dst,
                              double fx = 0, double fy = 0,
                              int interpolation = 0);

/**
 * 添加裁剪任务
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param src         输入源图像
 * @param dst         输出目标图像
 * @param rect        需要裁剪的矩形区域
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imcropTask(im_job_handle_t job_handle,
                            const rga_buffer_t src, rga_buffer_t dst, im_rect rect);

/**
 * 添加平移任务
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param src         输入源图像
 * @param dst         输出目标图像
 * @param x           目标图像 X 方向起始坐标
 * @param y           目标图像 Y 方向起始坐标
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imtranslateTask(im_job_handle_t job_handle,
                                 const rga_buffer_t src, rga_buffer_t dst, int x, int y);

/**
 * 添加格式转换任务
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param src         输入源图像
 * @param dst         输出目标图像
 * @param sfmt        源图像格式
 * @param dfmt        目标图像格式
 * @param mode        色彩空间模式
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imcvtcolorTask(im_job_handle_t job_handle,
                                rga_buffer_t src, rga_buffer_t dst,
                                int sfmt, int dfmt, int mode = IM_COLOR_SPACE_DEFAULT);

/**
 * 添加旋转任务
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param src         输入源图像
 * @param dst         输出目标图像
 * @param rotation    旋转角度
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imrotateTask(im_job_handle_t job_handle,
                              const rga_buffer_t src, rga_buffer_t dst, int rotation);

/**
 * 添加翻转任务
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param src         输入源图像
 * @param dst         输出目标图像
 * @param mode        翻转模式（水平/垂直）
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imflipTask(im_job_handle_t job_handle,
                            const rga_buffer_t src, rga_buffer_t dst, int mode);

/**
 * 添加双通道混合任务（SRC + DST -> DST）
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param fg_image    前景图像
 * @param bg_image    背景图像（也是输出目标）
 * @param mode        Porter-Duff 混合模式
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imblendTask(im_job_handle_t job_handle,
                             const rga_buffer_t fg_image, rga_buffer_t bg_image,
                             int mode = IM_ALPHA_BLEND_SRC_OVER);

/**
 * 添加三通道合成任务（SRCA + SRCB -> DST）
 *
 * @param job_handle    将任务插入到此 job 句柄中
 * @param fg_image      前景图像
 * @param bg_image      背景图像
 * @param output_image  输出目标图像
 * @param mode          Porter-Duff 混合模式
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imcompositeTask(im_job_handle_t job_handle,
                                 const rga_buffer_t fg_image, const rga_buffer_t bg_image,
                                 rga_buffer_t output_image,
                                 int mode = IM_ALPHA_BLEND_SRC_OVER);

/**
 * 添加色键抠图任务
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param fg_image    前景图像
 * @param bg_image    背景图像（也是输出目标）
 * @param range       色键范围
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imcolorkeyTask(im_job_handle_t job_handle,
                                const rga_buffer_t fg_image, rga_buffer_t bg_image,
                                im_colorkey_range range, int mode = IM_ALPHA_COLORKEY_NORMAL);

/**
 * 添加 OSD 叠加任务
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param osd         OSD 文本块图像
 * @param bg_image    背景图像
 * @param osd_rect    需要叠加 OSD 的矩形区域
 * @param osd_config  OSD 模式配置
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imosdTask(im_job_handle_t job_handle,
                           const rga_buffer_t osd,const rga_buffer_t bg_image,
                           const im_rect osd_rect, im_osd_t *osd_config);

/**
 * 添加 NN 量化任务
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param src         输入源图像
 * @param dst         输出目标图像
 * @param nn_info     NN 量化配置
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imquantizeTask(im_job_handle_t job_handle,
                                const rga_buffer_t src, rga_buffer_t dst, im_nn_t nn_info);

/**
 * 添加光栅操作（ROP）任务
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param src         输入源图像
 * @param dst         输出目标图像
 * @param rop_code    ROP 操作码
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imropTask(im_job_handle_t job_handle,
                           const rga_buffer_t src, rga_buffer_t dst, int rop_code);

/**
 * 添加颜色填充任务
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param dst         输出目标图像
 * @param rect        需要填充颜色的矩形区域
 * @param color       填充颜色值
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imfillTask(im_job_handle_t job_handle, rga_buffer_t dst, im_rect rect, uint32_t color);

/**
 * 添加批量颜色填充任务
 *
 * @param job_handle   将任务插入到此 job 句柄中
 * @param dst          输出目标图像
 * @param rect_array   需要填充颜色的矩形区域数组
 * @param array_size   矩形区域数组的大小
 * @param color        填充颜色值
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imfillTaskArray(im_job_handle_t job_handle,
                                 rga_buffer_t dst,
                                 im_rect *rect_array, int array_size, uint32_t color);

/**
 * 添加绘制矩形任务
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param dst         输出目标图像
 * @param rect        矩形区域
 * @param color       矩形颜色值
 * @param thickness   线条粗细，负值（如 -1）表示实心矩形
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imrectangleTask(im_job_handle_t job_handle,
                                 rga_buffer_t dst,
                                 im_rect rect,
                                 uint32_t color, int thickness);

/**
 * 添加批量绘制矩形任务
 *
 * @param job_handle   将任务插入到此 job 句柄中
 * @param dst          输出目标图像
 * @param rect_array   矩形区域数组
 * @param array_size   矩形区域数组的大小
 * @param color        矩形颜色值
 * @param thickness    线条粗细，负值（如 -1）表示实心矩形
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imrectangleTaskArray(im_job_handle_t job_handle,
                                      rga_buffer_t dst,
                                      im_rect *rect_array, int array_size,
                                      uint32_t color, int thickness);

/**
 * 添加马赛克任务
 *
 * @param job_handle   将任务插入到此 job 句柄中
 * @param image        输出目标图像
 * @param rect         需要打马赛克的矩形区域
 * @param mosaic_mode  马赛克块大小配置
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS immosaicTask(im_job_handle_t job_handle,
                              const rga_buffer_t image, im_rect rect, int mosaic_mode);

/**
 * 添加批量马赛克任务
 *
 * @param job_handle   将任务插入到此 job 句柄中
 * @param image        输出目标图像
 * @param rect_array   需要打马赛克的矩形区域数组
 * @param array_size   矩形区域数组的大小
 * @param mosaic_mode  马赛克块大小配置
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS immosaicTaskArray(im_job_handle_t job_handle,
                                   const rga_buffer_t image,
                                   im_rect *rect_array, int array_size, int mosaic_mode);

/**
 * 添加调色板任务
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param src         输入源图像
 * @param dst         输出目标图像
 * @param lut         LUT 查找表
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS impaletteTask(im_job_handle_t job_handle,
                               rga_buffer_t src, rga_buffer_t dst, rga_buffer_t lut);

/**
 * 添加复合处理任务（支持自定义操作组合）
 *
 * @param job_handle  将任务插入到此 job 句柄中
 * @param src         输入源图像（混合中作为背景）
 * @param dst         输出目标图像（混合中作为背景）
 * @param pat         前景图像或 LUT 查找表
 * @param srect       src 通道处理矩形区域
 * @param drect       dst 通道处理矩形区域
 * @param prect       pat 通道处理矩形区域
 * @param opt_ptr     图像处理选项配置
 * @param usage       图像处理模式标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS improcessTask(im_job_handle_t job_handle,
                               rga_buffer_t src, rga_buffer_t dst, rga_buffer_t pat,
                               im_rect srect, im_rect drect, im_rect prect,
                               im_opt_t *opt_ptr, int usage);

#endif /* #ifdef __cplusplus */

#endif /* #ifndef _im2d_task_h_ */
