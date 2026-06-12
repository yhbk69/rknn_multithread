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
#ifndef _im2d_single_h_
#define _im2d_single_h_

#include "im2d_type.h"

#ifdef __cplusplus

/**
 * 图像复制
 *
 * @param src
 *      输入源图像
 * @param dst
 *      输出目标图像
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 * @param release_fence_fd
 *      当 sync == 0 时，用于标识当前 job 状态的 fence fd
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imcopy(const rga_buffer_t src, rga_buffer_t dst, int sync = 1, int *release_fence_fd = NULL);

/**
 * 图像缩放
 *
 * @param src
 *      输入源图像
 * @param dst
 *      输出目标图像
 * @param fx
 *      X 方向缩放因子
 * @param fy
 *      Y 方向缩放因子
 * @param interpolation
 *      插值算法（仅 RGA1 支持）
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 * @param release_fence_fd
 *      当 sync == 0 时，用于标识当前 job 状态的 fence fd
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imresize(const rga_buffer_t src, rga_buffer_t dst, double fx = 0, double fy = 0, int interpolation = 0, int sync = 1, int *release_fence_fd = NULL);

/**
 * 图像裁剪
 *
 * @param src
 *      输入源图像
 * @param dst
 *      输出目标图像
 * @param rect
 *      源图像上需要裁剪的矩形区域
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 * @param release_fence_fd
 *      当 sync == 0 时，用于标识当前 job 状态的 fence fd
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imcrop(const rga_buffer_t src, rga_buffer_t dst, im_rect rect, int sync = 1, int *release_fence_fd = NULL);

/**
 * 图像平移
 *
 * @param src
 *      输入源图像
 * @param dst
 *      输出目标图像
 * @param x
 *      目标图像 X 方向起始点坐标
 * @param y
 *      目标图像 Y 方向起始点坐标
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 * @param release_fence_fd
 *      当 sync == 0 时，用于标识当前 job 状态的 fence fd
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imtranslate(const rga_buffer_t src, rga_buffer_t dst, int x, int y, int sync = 1, int *release_fence_fd = NULL);

/**
 * 格式转换
 *
 * @param src
 *      输入源图像
 * @param dst
 *      输出目标图像
 * @param sfmt
 *      源图像格式
 * @param dfmt
 *      目标图像格式
 * @param mode
 *      色彩空间模式:
 *          IM_YUV_TO_RGB_BT601_LIMIT
 *          IM_YUV_TO_RGB_BT601_FULL
 *          IM_YUV_TO_RGB_BT709_LIMIT
 *          IM_RGB_TO_YUV_BT601_FULL
 *          IM_RGB_TO_YUV_BT601_LIMIT
 *          IM_RGB_TO_YUV_BT709_LIMIT
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 * @param release_fence_fd
 *      当 sync == 0 时，用于标识当前 job 状态的 fence fd
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imcvtcolor(rga_buffer_t src, rga_buffer_t dst, int sfmt, int dfmt, int mode = IM_COLOR_SPACE_DEFAULT, int sync = 1, int *release_fence_fd = NULL);

/**
 * 图像旋转
 *
 * @param src
 *      输入源图像
 * @param dst
 *      输出目标图像
 * @param rotation
 *      旋转角度:
 *          IM_HAL_TRANSFORM_ROT_90
 *          IM_HAL_TRANSFORM_ROT_180
 *          IM_HAL_TRANSFORM_ROT_270
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 * @param release_fence_fd
 *      当 sync == 0 时，用于标识当前 job 状态的 fence fd
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imrotate(const rga_buffer_t src, rga_buffer_t dst, int rotation, int sync = 1, int *release_fence_fd = NULL);

/**
 * 图像翻转
 *
 * @param src
 *      输入源图像
 * @param dst
 *      输出目标图像
 * @param mode
 *      翻转模式:
 *          IM_HAL_TRANSFORM_FLIP_H  - 水平翻转
 *          IM_HAL_TRANSFORM_FLIP_V  - 垂直翻转
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 * @param release_fence_fd
 *      当 sync == 0 时，用于标识当前 job 状态的 fence fd
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imflip(const rga_buffer_t src, rga_buffer_t dst, int mode, int sync = 1, int *release_fence_fd = NULL);

/**
 * 双通道混合（SRC + DST -> DST 或 SRCA + SRCB -> DST）
 *
 * @param fg_image
 *      前景图像
 * @param bg_image
 *      背景图像，也是输出目标图像
 * @param mode
 *      Porter-Duff 混合模式:
 *          IM_ALPHA_BLEND_SRC
 *          IM_ALPHA_BLEND_DST
 *          IM_ALPHA_BLEND_SRC_OVER
 *          IM_ALPHA_BLEND_DST_OVER
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 * @param release_fence_fd
 *      当 sync == 0 时，用于标识当前 job 状态的 fence fd
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imblend(const rga_buffer_t fd_image, rga_buffer_t bg_image, int mode = IM_ALPHA_BLEND_SRC_OVER, int sync = 1, int *release_fence_fd = NULL);

/**
 * 三通道合成（SRCA + SRCB -> DST）
 *
 * @param fg_image
 *      前景图像
 * @param bg_image
 *      背景图像
 * @param output_image
 *      输出目标图像
 * @param mode
 *      Porter-Duff 混合模式:
 *          IM_ALPHA_BLEND_SRC
 *          IM_ALPHA_BLEND_DST
 *          IM_ALPHA_BLEND_SRC_OVER
 *          IM_ALPHA_BLEND_DST_OVER
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 * @param release_fence_fd
 *      当 sync == 0 时，用于标识当前 job 状态的 fence fd
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imcomposite(const rga_buffer_t srcA, const rga_buffer_t srcB, rga_buffer_t dst, int mode = IM_ALPHA_BLEND_SRC_OVER, int sync = 1, int *release_fence_fd = NULL);

/**
 * 色键抠图
 *
 * @param src
 *      前景图像
 * @param dst
 *      背景图像，也是输出目标图像
 * @param range
 *      色键范围
 * @param mode
 *      色键模式（IM_ALPHA_COLORKEY_NORMAL / IM_ALPHA_COLORKEY_INVERTED）
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imcolorkey(const rga_buffer_t src, rga_buffer_t dst, im_colorkey_range range, int mode = IM_ALPHA_COLORKEY_NORMAL, int sync = 1, int *release_fence_fd = NULL);

/**
 * OSD 叠加
 *
 * @param osd
 *      OSD 文本块图像
 * @param dst
 *      背景图像
 * @param osd_rect
 *      需要叠加 OSD 的矩形区域
 * @param osd_config
 *      OSD 模式配置
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imosd(const rga_buffer_t osd,const rga_buffer_t dst,
                       const im_rect osd_rect, im_osd_t *osd_config,
                       int sync = 1, int *release_fence_fd = NULL);

/**
 * NN 量化
 *
 * @param src
 *      输入源图像
 * @param dst
 *      输出目标图像
 * @param nn_info
 *      NN 量化配置（各通道缩放因子和偏移量）
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imquantize(const rga_buffer_t src, rga_buffer_t dst, im_nn_t nn_info, int sync = 1, int *release_fence_fd = NULL);

/**
 * 光栅操作（ROP）
 *
 * @param src
 *      输入源图像
 * @param dst
 *      输出目标图像
 * @param rop_code
 *      ROP 操作码（AND/OR/XOR/NOT 等）
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imrop(const rga_buffer_t src, rga_buffer_t dst, int rop_code, int sync = 1, int *release_fence_fd = NULL);

/**
 * 颜色填充/重置/绘制
 *
 * @param dst
 *      输出目标图像
 * @param rect
 *      需要填充颜色的矩形区域
 * @param color
 *      填充颜色值
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imfill(rga_buffer_t dst, im_rect rect, int color, int sync = 1, int *release_fence_fd = NULL);

/**
 * 批量颜色填充
 *
 * @param dst
 *      输出目标图像
 * @param rect_array
 *      需要填充颜色的矩形区域数组
 * @param array_size
 *      矩形区域数组的大小
 * @param color
 *      填充颜色值
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imfillArray(rga_buffer_t dst, im_rect *rect_array, int array_size, uint32_t color, int sync = 1, int *release_fence_fd = NULL);

/**
 * 绘制矩形
 *
 * @param dst
 *      输出目标图像
 * @param rect
 *      矩形区域
 * @param color
 *      矩形颜色值
 * @param thickness
 *      矩形线条粗细。负值（如 -1）表示绘制实心矩形
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imrectangle(rga_buffer_t dst, im_rect rect,
                             uint32_t color, int thickness,
                             int sync = 1, int *release_fence_fd = NULL);

/**
 * 批量绘制矩形
 *
 * @param dst
 *      输出目标图像
 * @param rect_array
 *      矩形区域数组
 * @param array_size
 *      矩形区域数组的大小
 * @param color
 *      矩形颜色值
 * @param thickness
 *      矩形线条粗细。负值（如 -1）表示绘制实心矩形
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS imrectangleArray(rga_buffer_t dst, im_rect *rect_array, int array_size,
                                   uint32_t color, int thickness,
                                   int sync = 1, int *release_fence_fd = NULL);

/**
 * 马赛克
 *
 * @param image
 *      输出目标图像
 * @param rect
 *      需要打马赛克的矩形区域
 * @param mosaic_mode
 *      马赛克块大小配置:
 *          IM_MOSAIC_8
 *          IM_MOSAIC_16
 *          IM_MOSAIC_32
 *          IM_MOSAIC_64
 *          IM_MOSAIC_128
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS immosaic(const rga_buffer_t image, im_rect rect, int mosaic_mode, int sync = 1, int *release_fence_fd = NULL);

/**
 * 批量马赛克
 *
 * @param image
 *      输出目标图像
 * @param rect_array
 *      需要打马赛克的矩形区域数组
 * @param array_size
 *      矩形区域数组的大小
 * @param mosaic_mode
 *      马赛克块大小配置:
 *          IM_MOSAIC_8
 *          IM_MOSAIC_16
 *          IM_MOSAIC_32
 *          IM_MOSAIC_64
 *          IM_MOSAIC_128
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS immosaicArray(const rga_buffer_t image, im_rect *rect_array, int array_size, int mosaic_mode, int sync = 1, int *release_fence_fd = NULL);

/**
 * 调色板
 *
 * @param src
 *      输入源图像
 * @param dst
 *      输出目标图像
 * @param lut
 *      LUT 查找表
 * @param sync
 *      当 sync == 1 时，等待操作完成后返回；否则立即返回
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS impalette(rga_buffer_t src, rga_buffer_t dst, rga_buffer_t lut, int sync = 1, int *release_fence_fd = NULL);

/**
 * 单任务模式图像处理（支持复合操作）
 *
 * @param src
 *      输入源图像（混合操作中作为背景）
 * @param dst
 *      输出目标图像（混合操作中作为背景）
 * @param pat
 *      前景图像或 LUT 查找表
 * @param srect
 *      src 通道上需要处理的矩形区域
 * @param drect
 *      dst 通道上需要处理的矩形区域
 * @param prect
 *      pat 通道上需要处理的矩形区域
 * @param opt_ptr
 *      图像处理选项配置（旋转、缩放、混合、OSD 等）
 * @param usage
 *      图像处理模式标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS improcess(rga_buffer_t src, rga_buffer_t dst, rga_buffer_t pat,
                           im_rect srect, im_rect drect, im_rect prect,
                           int acquire_fence_fd, int *release_fence_fd,
                           im_opt_t *opt_ptr, int usage);

/**
 * 制作边框
 *
 * @param src
 *      输入源图像
 * @param dst
 *      输出目标图像
 * @param top
 *      上边框像素数
 * @param bottom
 *      下边框像素数
 * @param left
 *      左边框像素数
 * @param right
 *      右边框像素数
 * @param border_type
 *      边框类型（常量/反射/环绕）
 * @param value
 *      边框填充的像素值
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_API IM_STATUS immakeBorder(rga_buffer_t src, rga_buffer_t dst,
                              int top, int bottom, int left, int right,
                              int border_type, int value = 0,
                              int sync = 1, int acquir_fence_fd = -1, int *release_fence_fd = NULL);

#endif /* #ifdef __cplusplus */

IM_C_API IM_STATUS immosaic(const rga_buffer_t image, im_rect rect, int mosaic_mode, int sync);
IM_C_API IM_STATUS imosd(const rga_buffer_t osd,const rga_buffer_t dst,
                         const im_rect osd_rect, im_osd_t *osd_config, int sync);
IM_C_API IM_STATUS improcess(rga_buffer_t src, rga_buffer_t dst, rga_buffer_t pat,
                             im_rect srect, im_rect drect, im_rect prect, int usage);

/* 以下为兼容宏函数而保留的底层 C 函数符号 */
IM_C_API IM_STATUS imcopy_t(const rga_buffer_t src, rga_buffer_t dst, int sync);
IM_C_API IM_STATUS imresize_t(const rga_buffer_t src, rga_buffer_t dst, double fx, double fy, int interpolation, int sync);
IM_C_API IM_STATUS imcrop_t(const rga_buffer_t src, rga_buffer_t dst, im_rect rect, int sync);
IM_C_API IM_STATUS imtranslate_t(const rga_buffer_t src, rga_buffer_t dst, int x, int y, int sync);
IM_C_API IM_STATUS imcvtcolor_t(rga_buffer_t src, rga_buffer_t dst, int sfmt, int dfmt, int mode, int sync);
IM_C_API IM_STATUS imrotate_t(const rga_buffer_t src, rga_buffer_t dst, int rotation, int sync);
IM_C_API IM_STATUS imflip_t (const rga_buffer_t src, rga_buffer_t dst, int mode, int sync);
IM_C_API IM_STATUS imblend_t(const rga_buffer_t srcA, const rga_buffer_t srcB, rga_buffer_t dst, int mode, int sync);
IM_C_API IM_STATUS imcolorkey_t(const rga_buffer_t src, rga_buffer_t dst, im_colorkey_range range, int mode, int sync);
IM_C_API IM_STATUS imquantize_t(const rga_buffer_t src, rga_buffer_t dst, im_nn_t nn_info, int sync);
IM_C_API IM_STATUS imrop_t(const rga_buffer_t src, rga_buffer_t dst, int rop_code, int sync);
IM_C_API IM_STATUS imfill_t(rga_buffer_t dst, im_rect rect, int color, int sync);
IM_C_API IM_STATUS impalette_t(rga_buffer_t src, rga_buffer_t dst, rga_buffer_t lut, int sync);
/* 以上为兼容宏函数而保留的底层 C 函数符号 */

#ifndef __cplusplus

#define RGA_GET_MIN(n1, n2) ((n1) < (n2) ? (n1) : (n2))

/**
 * 图像复制（C 兼容宏）
 *
 * @param src    输入源图像
 * @param dst    输出目标图像
 * @param sync   等待操作完成标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
#define imcopy(src, dst, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imcopy_t(src, dst, 1); \
        } else if (__argc == 1){ \
            __ret = imcopy_t(src, dst, (int)__args[RGA_GET_MIN(__argc, 0)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })

/**
 * 图像缩放（C 兼容宏）
 *
 * @param src           输入源图像
 * @param dst           输出目标图像
 * @param fx            X 方向缩放因子
 * @param fy            Y 方向缩放因子
 * @param interpolation 插值算法
 * @param sync          等待操作完成标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
#define imresize(src, dst, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        double __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(double); \
        if (__argc == 0) { \
            __ret = imresize_t(src, dst, 0, 0, INTER_LINEAR, 1); \
        } else if (__argc == 2){ \
            __ret = imresize_t(src, dst, __args[RGA_GET_MIN(__argc, 0)], __args[RGA_GET_MIN(__argc, 1)], INTER_LINEAR, 1); \
        } else if (__argc == 3){ \
            __ret = imresize_t(src, dst, __args[RGA_GET_MIN(__argc, 0)], __args[RGA_GET_MIN(__argc, 1)], (int)__args[RGA_GET_MIN(__argc, 2)], 1); \
        } else if (__argc == 4){ \
            __ret = imresize_t(src, dst, __args[RGA_GET_MIN(__argc, 0)], __args[RGA_GET_MIN(__argc, 1)], (int)__args[RGA_GET_MIN(__argc, 2)], (int)__args[RGA_GET_MIN(__argc, 3)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })

#define impyramid(src, dst, direction) \
        imresize_t(src, \
                   dst, \
                   direction == IM_UP_SCALE ? 0.5 : 2, \
                   direction == IM_UP_SCALE ? 0.5 : 2, \
                   INTER_LINEAR, 1)

/**
 * 格式转换（C 兼容宏）
 *
 * @param src   输入源图像
 * @param dst   输出目标图像
 * @param sfmt  源格式
 * @param dfmt  目标格式
 * @param mode  色彩空间模式（IM_COLOR_SPACE_MODE）
 * @param sync  等待操作完成标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
#define imcvtcolor(src, dst, sfmt, dfmt, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imcvtcolor_t(src, dst, sfmt, dfmt, IM_COLOR_SPACE_DEFAULT, 1); \
        } else if (__argc == 1){ \
            __ret = imcvtcolor_t(src, dst, sfmt, dfmt, (int)__args[RGA_GET_MIN(__argc, 0)], 1); \
        } else if (__argc == 2){ \
            __ret = imcvtcolor_t(src, dst, sfmt, dfmt, (int)__args[RGA_GET_MIN(__argc, 0)], (int)__args[RGA_GET_MIN(__argc, 1)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })

/**
 * 图像裁剪（C 兼容宏）
 *
 * @param src   输入源图像
 * @param dst   输出目标图像
 * @param rect  裁剪矩形区域
 * @param sync  等待操作完成标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
#define imcrop(src, dst, rect, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imcrop_t(src, dst, rect, 1); \
        } else if (__argc == 1){ \
            __ret = imcrop_t(src, dst, rect, (int)__args[RGA_GET_MIN(__argc, 0)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })

/**
 * 图像平移（C 兼容宏）
 *
 * @param src   输入源图像
 * @param dst   输出目标图像
 * @param x     X 方向偏移
 * @param y     Y 方向偏移
 * @param sync  等待操作完成标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
#define imtranslate(src, dst, x, y, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imtranslate_t(src, dst, x, y, 1); \
        } else if (__argc == 1){ \
            __ret = imtranslate_t(src, dst, x, y, (int)__args[RGA_GET_MIN(__argc, 0)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })

/**
 * 图像旋转（C 兼容宏）
 *
 * @param src       输入源图像
 * @param dst       输出目标图像
 * @param rotation  旋转角度
 * @param sync      等待操作完成标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
#define imrotate(src, dst, rotation, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imrotate_t(src, dst, rotation, 1); \
        } else if (__argc == 1){ \
            __ret = imrotate_t(src, dst, rotation, (int)__args[RGA_GET_MIN(__argc, 0)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })


/**
 * 图像翻转（C 兼容宏）
 *
 * @param src   输入源图像
 * @param dst   输出目标图像
 * @param mode  翻转模式（水平/垂直）
 * @param sync  等待操作完成标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
#define imflip(src, dst, mode, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imflip_t(src, dst, mode, 1); \
        } else if (__argc == 1){ \
            __ret = imflip_t(src, dst, mode, (int)__args[RGA_GET_MIN(__argc, 0)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })

/**
 * 图像混合（C 兼容宏，SRC + DST -> DST）
 *
 * @param srcA  源图像 A
 * @param dst   输出目标图像
 * @param mode  混合模式
 * @param sync  等待操作完成标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
#define imblend(srcA, dst, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        rga_buffer_t srcB; \
        memset(&srcB, 0x00, sizeof(rga_buffer_t)); \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imblend_t(srcA, srcB, dst, IM_ALPHA_BLEND_SRC_OVER, 1); \
        } else if (__argc == 1){ \
            __ret = imblend_t(srcA, srcB, dst, (int)__args[RGA_GET_MIN(__argc, 0)], 1); \
        } else if (__argc == 2){ \
            __ret = imblend_t(srcA, srcB, dst, (int)__args[RGA_GET_MIN(__argc, 0)], (int)__args[RGA_GET_MIN(__argc, 1)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })
#define imcomposite(srcA, srcB, dst, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imblend_t(srcA, srcB, dst, IM_ALPHA_BLEND_SRC_OVER, 1); \
        } else if (__argc == 1){ \
            __ret = imblend_t(srcA, srcB, dst, (int)__args[RGA_GET_MIN(__argc, 0)], 1); \
        } else if (__argc == 2){ \
            __ret = imblend_t(srcA, srcB, dst, (int)__args[RGA_GET_MIN(__argc, 0)], (int)__args[RGA_GET_MIN(__argc, 1)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })

/**
 * 色键抠图（C 兼容宏）
 *
 * @param src    输入源图像
 * @param dst    输出目标图像
 * @param range  色键范围
 * @param sync   等待操作完成标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
#define imcolorkey(src, dst, range, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imcolorkey_t(src, dst, range, IM_ALPHA_COLORKEY_NORMAL, 1); \
        } else if (__argc == 1){ \
            __ret = imcolorkey_t(src, dst, range, (int)__args[RGA_GET_MIN(__argc, 0)], 1); \
        } else if (__argc == 2){ \
            __ret = imcolorkey_t(src, dst, range, (int)__args[RGA_GET_MIN(__argc, 0)], (int)__args[RGA_GET_MIN(__argc, 1)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })

/**
 * NN 量化（C 兼容宏）
 *
 * @param src      输入源图像
 * @param dst      输出目标图像
 * @param nn_info  NN 量化配置
 * @param sync     等待操作完成标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
#define imquantize(src, dst, nn_info, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imquantize_t(src, dst, nn_info, 1); \
        } else if (__argc == 1){ \
            __ret = imquantize_t(src, dst, nn_info, (int)__args[RGA_GET_MIN(__argc, 0)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })


/**
 * 光栅操作 ROP（C 兼容宏）
 *
 * @param src       输入源图像
 * @param dst       输出目标图像
 * @param rop_code  ROP 操作码
 * @param sync      等待操作完成标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
#define imrop(src, dst, rop_code, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imrop_t(src, dst, rop_code, 1); \
        } else if (__argc == 1){ \
            __ret = imrop_t(src, dst, rop_code, (int)__args[RGA_GET_MIN(__argc, 0)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })

/**
 * 颜色填充/重置/绘制（C 兼容宏）
 *
 * @param buf    目标图像缓冲区
 * @param rect   矩形区域
 * @param color  填充颜色
 * @param sync   等待操作完成标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
#define imfill(buf, rect, color, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imfill_t(buf, rect, color, 1); \
        } else if (__argc == 1){ \
            __ret = imfill_t(buf, rect, color, (int)__args[RGA_GET_MIN(__argc, 0)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })

#define imreset(buf, rect, color, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imfill_t(buf, rect, color, 1); \
        } else if (__argc == 1){ \
            __ret = imfill_t(buf, rect, color, (int)__args[RGA_GET_MIN(__argc, 0)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })

#define imdraw(buf, rect, color, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imfill_t(buf, rect, color, 1); \
        } else if (__argc == 1){ \
            __ret = imfill_t(buf, rect, color, (int)__args[RGA_GET_MIN(__argc, 0)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })

/**
 * 调色板（C 兼容宏）
 *
 * @param src   输入源图像
 * @param dst   输出目标图像
 * @param lut   LUT 查找表
 * @param sync  等待操作完成标志
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
#define impalette(src, dst, lut,  ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_SUCCESS; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = impalette_t(src, dst, lut, 1); \
        } else if (__argc == 1){ \
            __ret = impalette_t(src, dst, lut, (int)__args[RGA_GET_MIN(__argc, 0)]); \
        } else { \
            __ret = IM_STATUS_INVALID_PARAM; \
            printf("无效参数\n"); \
        } \
        __ret; \
    })
/* IM2D C 宏 API 定义结束 */
#endif

#endif /* #ifndef _im2d_single_h_ */