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

#ifndef _rk_drm_rga_
#define _rk_drm_rga_

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>

#include "rga.h"

#ifdef ANDROID
#define DRMRGA_HARDWARE_MODULE_ID "librga"

#include <hardware/gralloc.h>
#include <hardware/hardware.h>
#include <system/graphics.h>
#include <cutils/native_handle.h>

#ifdef ANDROID_12
#include <hardware/hardware_rockchip.h>
#endif

#endif

#define RGA_BLIT_SYNC   0x5017
#define RGA_BLIT_ASYNC  0x5018

#ifndef ANDROID /* LINUX */
/* 沿垂直轴水平翻转源图像 */
#define HAL_TRANSFORM_FLIP_H     0x01
/* 沿水平轴垂直翻转源图像 */
#define HAL_TRANSFORM_FLIP_V     0x02
/* 顺时针旋转源图像 90 度 */
#define HAL_TRANSFORM_ROT_90     0x04
/* 旋转源图像 180 度 */
#define HAL_TRANSFORM_ROT_180    0x03
/* 顺时针旋转源图像 270 度 */
#define HAL_TRANSFORM_ROT_270    0x07
#endif

#define HAL_TRANSFORM_FLIP_H_V   0x08

/*****************************************************************************/

/* for compatibility */
#define DRM_RGA_MODULE_API_VERSION      HWC_MODULE_API_VERSION_0_1
#define DRM_RGA_DEVICE_API_VERSION      HWC_DEVICE_API_VERSION_0_1
#define DRM_RGA_API_VERSION             HWC_DEVICE_API_VERSION

#define DRM_RGA_TRANSFORM_ROT_MASK      0x0000000F
#define DRM_RGA_TRANSFORM_ROT_0         0x00000000
#define DRM_RGA_TRANSFORM_ROT_90        HAL_TRANSFORM_ROT_90
#define DRM_RGA_TRANSFORM_ROT_180       HAL_TRANSFORM_ROT_180
#define DRM_RGA_TRANSFORM_ROT_270       HAL_TRANSFORM_ROT_270

#define DRM_RGA_TRANSFORM_FLIP_MASK     0x00000003
#define DRM_RGA_TRANSFORM_FLIP_H        HAL_TRANSFORM_FLIP_H
#define DRM_RGA_TRANSFORM_FLIP_V        HAL_TRANSFORM_FLIP_V

enum {
    AWIDTH                      = 0,
    AHEIGHT,
    ASTRIDE,
    AFORMAT,
    ASIZE,
    ATYPE,
};
/*****************************************************************************/

#ifndef ANDROID /* LINUX */
/* 内存类型定义 */
enum drm_rockchip_gem_mem_type {
    /* 物理连续内存，默认使用此类型 */
    ROCKCHIP_BO_CONTIG  = 1 << 0,
    /* 可缓存映射 */
    ROCKCHIP_BO_CACHABLE    = 1 << 1,
    /* 写合并映射 */
    ROCKCHIP_BO_WC      = 1 << 2,
    /* 安全内存（仅 TrustZone 可访问） */
    ROCKCHIP_BO_SECURE  = 1 << 3,
    /* 掩码，用于提取内存类型标志位 */
    ROCKCHIP_BO_MASK    = ROCKCHIP_BO_CONTIG | ROCKCHIP_BO_CACHABLE |
                ROCKCHIP_BO_WC | ROCKCHIP_BO_SECURE
};

typedef struct bo {
    int fd;
    void *ptr;
    size_t size;
    size_t offset;
    size_t pitch;
    unsigned handle;
} bo_t;
#endif

/*
   @value size:     用户无需关心此字段，用于防止内存越界读写
 */
typedef struct rga_rect {
    int xoffset;
    int yoffset;
    int width;
    int height;
    int wstride;
    int hstride;
    int format;
    int size;
} rga_rect_t;

typedef struct rga_nn {
    int nn_flag;
    int scale_r;
    int scale_g;
    int scale_b;
    int offset_r;
    int offset_g;
    int offset_b;
} rga_nn_t;

typedef struct rga_dither {
    int enable;
    int mode;
    int lut0_l;
    int lut0_h;
    int lut1_l;
    int lut1_h;
} rga_dither_t;

struct rga_mosaic_info {
    uint8_t enable;
    uint8_t mode;
};

struct rga_pre_intr_info {
    uint8_t enable;

    uint8_t read_intr_en;
    uint8_t write_intr_en;
    uint8_t read_hold_en;
    uint32_t read_threshold;
    uint32_t write_start;
    uint32_t write_step;
};

/* MAX(min, (max - channel_value)) */
struct rga_osd_invert_factor {
    uint8_t alpha_max;
    uint8_t alpha_min;
    uint8_t yg_max;
    uint8_t yg_min;
    uint8_t crb_max;
    uint8_t crb_min;
};

struct rga_color {
    union {
        struct {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            uint8_t alpha;
        };
        uint32_t value;
    };
};

struct rga_osd_bpp2 {
    uint8_t  ac_swap;           // AC 交换标志
                                // 0: CA 顺序
                                // 1: AC 顺序
    uint8_t  endian_swap;       // rgba2bpp 字节序交换
                                // 0: 大端序
                                // 1: 小端序
    struct rga_color color0;
    struct rga_color color1;
};

struct rga_osd_mode_ctrl {
    uint8_t mode;               // OSD 计算模式:
                                //   0b'1: 统计模式
                                //   1b'1: 自动反转叠加模式
    uint8_t direction_mode;     // 水平或垂直方向
                                //   0: 水平
                                //   1: 垂直
    uint8_t width_mode;         // 使用固定宽度或 LUT 宽度
                                //   0: 固定宽度
                                //   1: LUT 宽度
    uint16_t block_fix_width;   // OSD 块固定宽度
                                //   实际宽度 = (fix_width + 1) * 2
    uint8_t block_num;          // OSD 块数量
    uint16_t flags_index;       // 自动反转标志索引

    /* 反转配置 */
    uint8_t color_mode;         // 颜色选择
                                //   0: 使用 src1 颜色
                                //   1: 使用配置数据颜色
    uint8_t invert_flags_mode;  // 反转标志选择
                                //   0: 使用 RAM 标志
                                //   1: 使用上次结果
    uint8_t default_color_sel;  // 默认颜色模式
                                //   0: 默认亮色
                                //   1: 默认暗色
    uint8_t invert_enable;      // 反转通道使能
                                //   1 << 0: Alpha 通道使能
                                //   1 << 1: Y/G 通道禁用
                                //   1 << 2: C/RB 通道禁用
    uint8_t invert_mode;        // 反转计算模式
                                //   0: 普通(max-data)
                                //   1: 交换
    uint8_t invert_thresh;      // 若亮度 > 阈值，osd_flag 置为 1
    uint8_t unfix_index;        // OSD 宽度配置索引
};

struct rga_osd_info {
    uint8_t  enable;

    struct rga_osd_mode_ctrl mode_ctrl;
    struct rga_osd_invert_factor cal_factor;
    struct rga_osd_bpp2 bpp2_info;

    union {
        struct {
            uint32_t last_flags1;
            uint32_t last_flags0;
        };
        uint64_t last_flags;
    };

    union {
        struct {
            uint32_t cur_flags1;
            uint32_t cur_flags0;
        };
        uint64_t cur_flags;
    };
};

/*
   @value fd:     通过 fd 共享内存，支持 ion 共享 fd 和 dma fd
   @value virAddr: 用户空间虚拟地址
   @value phyAddr: 物理地址
   @value hnd:     使用 buffer_handle_t
 */
typedef struct rga_info {
    int fd;
    void *virAddr;
    void *phyAddr;
#ifndef ANDROID /* LINUX */
    unsigned hnd;
#else /* Android */
    buffer_handle_t hnd;
#endif
    int format;
    rga_rect_t rect;
    unsigned int blend;
    int bufferSize;
    int rotation;
    int color;
    int testLog;
    int mmuFlag;
    int colorkey_en;
    int colorkey_mode;
    int colorkey_max;
    int colorkey_min;
    int scale_mode;
    int color_space_mode;
    int sync_mode;
    rga_nn_t nn;
    rga_dither_t dither;
    int rop_code;
    int rd_mode;
    unsigned short is_10b_compact;
    unsigned short is_10b_endian;

    int in_fence_fd;
    int out_fence_fd;

    int core;
    int priority;

    unsigned short enable;

    int handle;

    struct rga_mosaic_info mosaic_info;

    struct rga_osd_info osd_info;

    struct rga_pre_intr_info pre_intr;

    int mpi_mode;

    union {
        int ctx_id;
        int job_handle;
    };

    char reserve[402];
} rga_info_t;


typedef struct drm_rga {
    rga_rect_t src;
    rga_rect_t dst;
} drm_rga_t;

/*
   @fun rga_set_rect: 便捷设置矩形区域

   @param rect: 要设置的矩形结构体指针
   示例 — 设置源矩形：
     drm_rga_t rects;
     rga_set_rect(&rects.src, 0, 0, 1920, 1080, 1920, 1920, RK_FORMAT_NV12);
     表示将源矩形设置为：左上角(0,0)，宽1920，高1080，步长1920x1920，格式NV12
 */
static inline int rga_set_rect(rga_rect_t *rect,
                               int x, int y, int w, int h, int sw, int sh, int f) {
    if (!rect)
        return -EINVAL;

    rect->xoffset = x;
    rect->yoffset = y;
    rect->width = w;
    rect->height = h;
    rect->wstride = sw;
    rect->hstride = sh;
    rect->format = f;

    return 0;
}

#ifndef ANDROID /* LINUX */
static inline void rga_set_rotation(rga_info_t *info, int angle) {
    if (angle == 90)
        info->rotation = HAL_TRANSFORM_ROT_90;
    else if (angle == 180)
        info->rotation = HAL_TRANSFORM_ROT_180;
    else if (angle == 270)
        info->rotation = HAL_TRANSFORM_ROT_270;
}
#endif
/*****************************************************************************/

#endif
