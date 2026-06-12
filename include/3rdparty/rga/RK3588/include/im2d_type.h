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

#ifndef _RGA_IM2D_TYPE_H_
#define _RGA_IM2D_TYPE_H_

#include <stdint.h>

#include "rga.h"

#define IM_API /* 按需定义 API 导出属性 */

#ifdef __cplusplus
#define IM_C_API extern "C"
#define IM_EXPORT_API extern "C"
#else
#define IM_C_API
#define IM_EXPORT_API
#endif

#ifdef __cplusplus
#define DEFAULT_INITIALIZER(x) = x
#else
#define DEFAULT_INITIALIZER(x)
#endif

typedef uint32_t im_api_version_t;
typedef uint32_t im_job_handle_t;
typedef uint32_t im_ctx_id_t;
typedef uint32_t rga_buffer_handle_t;

typedef enum {
    /* 旋转 */
    IM_HAL_TRANSFORM_ROT_90     = 1 << 0,
    IM_HAL_TRANSFORM_ROT_180    = 1 << 1,
    IM_HAL_TRANSFORM_ROT_270    = 1 << 2,
    IM_HAL_TRANSFORM_FLIP_H     = 1 << 3,
    IM_HAL_TRANSFORM_FLIP_V     = 1 << 4,
    IM_HAL_TRANSFORM_FLIP_H_V   = 1 << 5,
    IM_HAL_TRANSFORM_MASK       = 0x3f,

    /*
     * 混合（Blend）
     * 额外的混合模式，可与源和目标配置同时使用。
     * 若未设置以下任何标志，默认使用「SRC over DST」模式。
     */
    IM_ALPHA_BLEND_SRC_OVER     = 1 << 6,     /* 默认,Porter-Duff「SRC over DST」 */
    IM_ALPHA_BLEND_SRC          = 1 << 7,     /* Porter-Duff「SRC」 */
    IM_ALPHA_BLEND_DST          = 1 << 8,     /* Porter-Duff「DST」 */
    IM_ALPHA_BLEND_SRC_IN       = 1 << 9,     /* Porter-Duff「SRC in DST」 */
    IM_ALPHA_BLEND_DST_IN       = 1 << 10,    /* Porter-Duff「DST in SRC」 */
    IM_ALPHA_BLEND_SRC_OUT      = 1 << 11,    /* Porter-Duff「SRC out DST」 */
    IM_ALPHA_BLEND_DST_OUT      = 1 << 12,    /* Porter-Duff「DST out SRC」 */
    IM_ALPHA_BLEND_DST_OVER     = 1 << 13,    /* Porter-Duff「DST over SRC」 */
    IM_ALPHA_BLEND_SRC_ATOP     = 1 << 14,    /* Porter-Duff「SRC ATOP」 */
    IM_ALPHA_BLEND_DST_ATOP     = 1 << 15,    /* Porter-Duff「DST ATOP」 */
    IM_ALPHA_BLEND_XOR          = 1 << 16,    /* 异或 */
    IM_ALPHA_BLEND_MASK         = 0x1ffc0,

    IM_ALPHA_COLORKEY_NORMAL    = 1 << 17,
    IM_ALPHA_COLORKEY_INVERTED  = 1 << 18,
    IM_ALPHA_COLORKEY_MASK      = 0x60000,

    IM_SYNC                     = 1 << 19,
    IM_CROP                     = 1 << 20,    /* 预留，未使用 */
    IM_COLOR_FILL               = 1 << 21,
    IM_COLOR_PALETTE            = 1 << 22,
    IM_NN_QUANTIZE              = 1 << 23,
    IM_ROP                      = 1 << 24,
    IM_ALPHA_BLEND_PRE_MUL      = 1 << 25,
    IM_ASYNC                    = 1 << 26,
    IM_MOSAIC                   = 1 << 27,
    IM_OSD                      = 1 << 28,
    IM_PRE_INTR                 = 1 << 29,
} IM_USAGE;

typedef enum {
    IM_RASTER_MODE              = 1 << 0,
    IM_FBC_MODE                 = 1 << 1,
    IM_TILE_MODE                = 1 << 2,
} IM_RD_MODE;

typedef enum {
    IM_SCHEDULER_RGA3_CORE0     = 1 << 0,
    IM_SCHEDULER_RGA3_CORE1     = 1 << 1,
    IM_SCHEDULER_RGA2_CORE0     = 1 << 2,
    IM_SCHEDULER_RGA3_DEFAULT   = IM_SCHEDULER_RGA3_CORE0,
    IM_SCHEDULER_RGA2_DEFAULT   = IM_SCHEDULER_RGA2_CORE0,
    IM_SCHEDULER_MASK           = 0x7,
    IM_SCHEDULER_DEFAULT        = 0,
} IM_SCHEDULER_CORE;

typedef enum {
    IM_ROP_AND                  = 0x88,
    IM_ROP_OR                   = 0xee,
    IM_ROP_NOT_DST              = 0x55,
    IM_ROP_NOT_SRC              = 0x33,
    IM_ROP_XOR                  = 0xf6,
    IM_ROP_NOT_XOR              = 0xf9,
} IM_ROP_CODE;

typedef enum {
    IM_MOSAIC_8                 = 0x0,
    IM_MOSAIC_16                = 0x1,
    IM_MOSAIC_32                = 0x2,
    IM_MOSAIC_64                = 0x3,
    IM_MOSAIC_128               = 0x4,
} IM_MOSAIC_MODE;

typedef enum {
    IM_BORDER_CONSTANT = 0,             /* 常量填充边框：iiiiii|abcdefgh|iiiiiii（用指定值 'i' 填充） */
    IM_BORDER_REFLECT = 2,              /* 镜像填充边框：fedcba|abcdefgh|hgfedcb */
    IM_BORDER_WRAP = 3,                 /* 环绕填充边框：cdefgh|abcdefgh|abcdefg */
} IM_BORDER_TYPE;

/* 状态码，由所有 blit 函数返回 */
typedef enum {
    IM_YUV_TO_RGB_BT601_LIMIT   = 1 << 0,
    IM_YUV_TO_RGB_BT601_FULL    = 2 << 0,
    IM_YUV_TO_RGB_BT709_LIMIT   = 3 << 0,
    IM_YUV_TO_RGB_MASK          = 3 << 0,
    IM_RGB_TO_YUV_BT601_FULL    = 1 << 2,
    IM_RGB_TO_YUV_BT601_LIMIT   = 2 << 2,
    IM_RGB_TO_YUV_BT709_LIMIT   = 3 << 2,
    IM_RGB_TO_YUV_MASK          = 3 << 2,
    IM_RGB_TO_Y4                = 1 << 4,
    IM_RGB_TO_Y4_DITHER         = 2 << 4,
    IM_RGB_TO_Y1_DITHER         = 3 << 4,
    IM_Y4_MASK                  = 3 << 4,
    IM_RGB_FULL                 = 1 << 8,
    IM_RGB_CLIP                 = 2 << 8,
    IM_YUV_BT601_LIMIT_RANGE    = 3 << 8,
    IM_YUV_BT601_FULL_RANGE     = 4 << 8,
    IM_YUV_BT709_LIMIT_RANGE    = 5 << 8,
    IM_YUV_BT709_FULL_RANGE     = 6 << 8,
    IM_FULL_CSC_MASK            = 0xf << 8,
    IM_COLOR_SPACE_DEFAULT      = 0,
} IM_COLOR_SPACE_MODE;

typedef enum {
    IM_UP_SCALE,
    IM_DOWN_SCALE,
} IM_SCALE;

typedef enum {
    INTER_NEAREST,
    INTER_LINEAR,
    INTER_CUBIC,
} IM_SCALE_MODE;

typedef enum {
    IM_CONFIG_SCHEDULER_CORE,
    IM_CONFIG_PRIORITY,
    IM_CONFIG_CHECK,
} IM_CONFIG_NAME;

typedef enum {
    IM_OSD_MODE_STATISTICS      = 0x1 << 0,
    IM_OSD_MODE_AUTO_INVERT     = 0x1 << 1,
} IM_OSD_MODE;

typedef enum {
    IM_OSD_INVERT_CHANNEL_NONE          = 0x0,
    IM_OSD_INVERT_CHANNEL_Y_G           = 0x1 << 0,
    IM_OSD_INVERT_CHANNEL_C_RB          = 0x1 << 1,
    IM_OSD_INVERT_CHANNEL_ALPHA         = 0x1 << 2,
    IM_OSD_INVERT_CHANNEL_COLOR         = IM_OSD_INVERT_CHANNEL_Y_G |
                                          IM_OSD_INVERT_CHANNEL_C_RB,
    IM_OSD_INVERT_CHANNEL_BOTH          = IM_OSD_INVERT_CHANNEL_COLOR |
                                          IM_OSD_INVERT_CHANNEL_ALPHA,
} IM_OSD_INVERT_CHANNEL;

typedef enum {
    IM_OSD_FLAGS_INTERNAL = 0,
    IM_OSD_FLAGS_EXTERNAL,
} IM_OSD_FLAGS_MODE;

typedef enum {
    IM_OSD_INVERT_USE_FACTOR,
    IM_OSD_INVERT_USE_SWAP,
} IM_OSD_INVERT_MODE;

typedef enum {
    IM_OSD_BACKGROUND_DEFAULT_BRIGHT = 0,
    IM_OSD_BACKGROUND_DEFAULT_DARK,
} IM_OSD_BACKGROUND_DEFAULT;

typedef enum {
    IM_OSD_BLOCK_MODE_NORMAL = 0,
    IM_OSD_BLOCK_MODE_DIFFERENT,
} IM_OSD_BLOCK_WIDTH_MODE;

typedef enum {
    IM_OSD_MODE_HORIZONTAL,
    IM_OSD_MODE_VERTICAL,
} IM_OSD_DIRECTION;

typedef enum {
    IM_OSD_COLOR_PIXEL,
    IM_OSD_COLOR_EXTERNAL,
} IM_OSD_COLOR_MODE;

typedef enum {
    IM_INTR_READ_INTR           = 1 << 0,
    IM_INTR_READ_HOLD           = 1 << 1,
    IM_INTR_WRITE_INTR          = 1 << 2,
} IM_PRE_INTR_FLAGS;

typedef enum {
    IM_CONTEXT_NONE             = 0x0,           /* 无上下文 */
    IM_CONTEXT_SRC_FIX_ENABLE   = 0x1 << 0,     /* 允许内核修改源通道图像参数 */
    IM_CONTEXT_SRC_CACHE_INFO   = 0x1 << 1,     /* 用修改后的参数替换 ctx 中的参数 */
    IM_CONTEXT_SRC1_FIX_ENABLE  = 0x1 << 2,     /* 允许内核修改源 1 通道图像参数 */
    IM_CONTEXT_SRC1_CACHE_INFO  = 0x1 << 3,     /* 用修改后的参数替换 ctx 中的源 1 参数 */
    IM_CONTEXT_DST_FIX_ENABLE   = 0x1 << 4,     /* 允许内核修改目标通道图像参数 */
    IM_CONTEXT_DST_CACHE_INFO   = 0x1 << 5,     /* 用修改后的参数替换 ctx 中的目标参数 */
} IM_CONTEXT_FLAGS;

/* 获取 RGA 基本信息的索引 */
typedef enum {
    RGA_VENDOR = 0,
    RGA_VERSION,
    RGA_MAX_INPUT,
    RGA_MAX_OUTPUT,
    RGA_BYTE_STRIDE,
    RGA_SCALE_LIMIT,
    RGA_INPUT_FORMAT,
    RGA_OUTPUT_FORMAT,
    RGA_FEATURE,
    RGA_EXPECTED,
    RGA_ALL,
} IM_INFORMATION;

/* 状态码，由所有 blit 函数返回 */
typedef enum {
    IM_STATUS_NOERROR           =  2,  /* 无错误 */
    IM_STATUS_SUCCESS           =  1,  /* 成功 */
    IM_STATUS_NOT_SUPPORTED     = -1,  /* 不支持的操作 */
    IM_STATUS_OUT_OF_MEMORY     = -2,  /* 内存不足 */
    IM_STATUS_INVALID_PARAM     = -3,  /* 无效参数 */
    IM_STATUS_ILLEGAL_PARAM     = -4,  /* 非法参数 */
    IM_STATUS_ERROR_VERSION     = -5,  /* 版本错误 */
    IM_STATUS_FAILED            =  0,  /* 失败 */
} IM_STATUS;

/* 矩形区域定义 */
typedef struct {
    int x;        /* 左上角 x 坐标 */
    int y;        /* 左上角 y 坐标 */
    int width;    /* 宽度 */
    int height;   /* 高度 */
} im_rect;

typedef struct {
    int max;                    /* 色键最大值 */
    int min;                    /* 色键最小值 */
} im_colorkey_range;


typedef struct im_nn {
    int scale_r;                /* R 通道缩放因子 */
    int scale_g;                /* G 通道缩放因子 */
    int scale_b;                /* B 通道缩放因子 */
    int offset_r;               /* R 通道偏移量 */
    int offset_g;               /* G 通道偏移量 */
    int offset_b;               /* B 通道偏移量 */
} im_nn_t;

/* 图像信息结构体定义 */
typedef struct {
    void* vir_addr;                     /* 虚拟地址 */
    void* phy_addr;                     /* 物理地址 */
    int fd;                             /* 共享 fd */

    int width;                          /* 宽度 */
    int height;                         /* 高度 */
    int wstride;                        /* 宽度方向步长 */
    int hstride;                        /* 高度方向步长 */
    int format;                         /* 像素格式 */

    int color_space_mode;               /* 色彩空间模式 */
    int global_alpha;                   /* 全局 Alpha 值 */
    int rd_mode;                        /* 读写模式 */

    /* 兼容旧版 */
    int color;                          /* 颜色值，用于颜色填充 */
    im_colorkey_range colorkey_range;   /* 色键范围 */
    im_nn_t nn;
    int rop_code;

    rga_buffer_handle_t handle;         /* 缓冲区句柄 */
} rga_buffer_t;

typedef struct im_color {
    union {
        struct {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            uint8_t alpha;
        };
        uint32_t value;
    };
} im_color_t;

typedef struct im_osd_invert_factor {
    uint8_t alpha_max;
    uint8_t alpha_min;
    uint8_t yg_max;
    uint8_t yg_min;
    uint8_t crb_max;
    uint8_t crb_min;
} im_osd_invert_factor_t;

typedef struct im_osd_bpp2 {
    uint8_t  ac_swap;       // AC 交换标志
                            // 0: CA 顺序
                            // 1: AC 顺序
    uint8_t  endian_swap;   // rgba2bpp 字节序交换
                            // 0: 大端序
                            // 1: 小端序
    im_color_t color0;
    im_color_t color1;
} im_osd_bpp2_t;

typedef struct im_osd_block {
    int width_mode;                 // 普通或差异模式
                                    //   IM_OSD_BLOCK_MODE_NORMAL
                                    //   IM_OSD_BLOCK_MODE_DIFFERENT
    union {
        int width;                  // 普通模式下的块宽度
        int width_index;            // 差异模式下在 RAM 中的块宽度索引
    };

    int block_count;                // 块数量

    int background_config;          // 背景配置：亮色或暗色
                                    //   IM_OSD_BACKGROUND_DEFAULT_BRIGHT
                                    //   IM_OSD_BACKGROUND_DEFAULT_DARK

    int direction;                  // OSD 块方向
                                    //   IM_OSD_MODE_HORIZONTAL
                                    //   IM_OSD_MODE_VERTICAL

    int color_mode;                 // 使用 src1 颜色或配置颜色
                                    //   IM_OSD_COLOR_PIXEL
                                    //   IM_OSD_COLOR_EXTERNAL
    im_color_t normal_color;        // 配置颜色：普通
    im_color_t invert_color;        // 配置颜色：反转
} im_osd_block_t;

typedef struct im_osd_invert {
    int invert_channel;         // 反转通道配置:
                                //   IM_OSD_INVERT_CHANNEL_NONE
                                //   IM_OSD_INVERT_CHANNEL_Y_G
                                //   IM_OSD_INVERT_CHANNEL_C_RB
                                //   IM_OSD_INVERT_CHANNEL_ALPHA
                                //   IM_OSD_INVERT_CHANNEL_COLOR
                                //   IM_OSD_INVERT_CHANNEL_BOTH
    int flags_mode;             // 使用外部或内部 RAM 反转标志
                                //   IM_OSD_FLAGS_EXTERNAL
                                //   IM_OSD_FLAGS_INTERNAL
    int flags_index;            // 使用内部 RAM 反转标志时的标志索引

    uint64_t invert_flags;      // 外部反转标志
    uint64_t current_flags;     // 当前标志

    int invert_mode;            // 反转使用交换或因子模式
                                //   IM_OSD_INVERT_USE_FACTOR
                                //   IM_OSD_INVERT_USE_SWAP
    im_osd_invert_factor_t factor;

    int threash;
} im_osd_invert_t;

typedef struct im_osd {
    int osd_mode;                       // OSD 模式：统计或自动反转
                                        //   IM_OSD_MODE_STATISTICS
                                        //   IM_OSD_MODE_AUTO_INVERT
    im_osd_block_t block_parm;          // OSD 块信息配置

    im_osd_invert_t invert_config;

    im_osd_bpp2_t bpp2_info;
} im_osd_t;

typedef struct im_intr_config {
    uint32_t flags;

    int read_threshold;
    int write_start;
    int write_step;
} im_intr_config_t;

typedef struct im_opt {
    im_api_version_t version DEFAULT_INITIALIZER(RGA_CURRENT_API_HEADER_VERSION);

    int color;                          /* 颜色值，用于颜色填充 */
    im_colorkey_range colorkey_range;   /* 色键范围 */
    im_nn_t nn;
    int rop_code;

    int priority;                       /* 任务优先级 */
    int core;                           /* 指定调度核心 */

    int mosaic_mode;                    /* 马赛克模式 */

    im_osd_t osd_config;                /* OSD 配置 */

    im_intr_config_t intr_config;       /* 中断配置 */

    char reserve[128];                  /* 预留 */
} im_opt_t;

typedef struct im_handle_param {
    uint32_t width;
    uint32_t height;
    uint32_t format;
} im_handle_param_t;

#endif /* _RGA_IM2D_TYPE_H_ */
