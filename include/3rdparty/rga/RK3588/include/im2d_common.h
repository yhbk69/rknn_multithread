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
#ifndef _im2d_common_h_
#define _im2d_common_h_

#include "im2d_type.h"

/**
 * 查询 RGA 基本信息（版本、支持的分辨率、格式等）
 *
 * @param name
 *      RGA_VENDOR        - 厂商信息
 *      RGA_VERSION       - 版本号
 *      RGA_MAX_INPUT     - 最大输入分辨率
 *      RGA_MAX_OUTPUT    - 最大输出分辨率
 *      RGA_INPUT_FORMAT  - 支持的输入格式
 *      RGA_OUTPUT_FORMAT - 支持的输出格式
 *      RGA_EXPECTED      - 预期的行为
 *      RGA_ALL           - 全部信息
 *
 * @returns RGA 属性描述字符串
 */
IM_EXPORT_API const char* querystring(int name);

/**
 * 将错误码转换为可读的错误信息字符串
 *
 * @param status
 *      操作返回的状态码
 *
 * @returns 错误信息字符串
 */
#define imStrError(...) \
    ({ \
        const char* im2d_api_err; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            im2d_api_err = imStrError_t(IM_STATUS_INVALID_PARAM); \
        } else if (__argc == 1){ \
            im2d_api_err = imStrError_t((IM_STATUS)__args[0]); \
        } else { \
            im2d_api_err = ("Fatal error, imStrError() too many parameters\n"); \
            printf("致命错误, imStrError() 参数过多\n"); \
        } \
        im2d_api_err; \
    })
IM_C_API const char* imStrError_t(IM_STATUS status);

/**
 * 校验 im2d API 头文件版本是否与运行库匹配
 *
 * @param header_version
 *      默认值为 RGA_CURRENT_API_HEADER_VERSION，无特殊情况无需修改
 *
 * @returns 匹配成功返回 IM_STATUS_NOERROR，否则返回负错误码
 */
#ifdef __cplusplus
IM_API IM_STATUS imcheckHeader(im_api_version_t header_version = RGA_CURRENT_API_HEADER_VERSION);
#endif

/**
 * 校验 RGA 操作参数是否合法（分辨率、格式、模式等）
 *
 * @param src        源图像缓冲区
 * @param dst        目标图像缓冲区
 * @param pat        模板/前景图像缓冲区
 * @param src_rect   源图像操作区域
 * @param dst_rect   目标图像操作区域
 * @param pat_rect   模板图像操作区域
 * @param mode_usage 操作模式标志
 *
 * @returns 参数合法返回 IM_STATUS_NOERROR，否则返回负错误码
 */
#define imcheck(src, dst, src_rect, dst_rect, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_NOERROR; \
        rga_buffer_t __pat; \
        im_rect __pat_rect; \
        memset(&__pat, 0, sizeof(rga_buffer_t)); \
        memset(&__pat_rect, 0, sizeof(im_rect)); \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imcheck_t(src, dst, __pat, src_rect, dst_rect, __pat_rect, 0); \
        } else if (__argc == 1){ \
            __ret = imcheck_t(src, dst, __pat, src_rect, dst_rect, __pat_rect, __args[0]); \
        } else { \
            __ret = IM_STATUS_FAILED; \
            printf("参数校验失败\n"); \
        } \
        __ret; \
    })
#define imcheck_composite(src, dst, pat, src_rect, dst_rect, pat_rect, ...) \
    ({ \
        IM_STATUS __ret = IM_STATUS_NOERROR; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            __ret = imcheck_t(src, dst, pat, src_rect, dst_rect, pat_rect, 0); \
        } else if (__argc == 1){ \
            __ret = imcheck_t(src, dst, pat, src_rect, dst_rect, pat_rect, __args[0]); \
        } else { \
            __ret = IM_STATUS_FAILED; \
            printf("参数校验失败\n"); \
        } \
        __ret; \
    })
IM_C_API IM_STATUS imcheck_t(const rga_buffer_t src, const rga_buffer_t dst, const rga_buffer_t pat,
                             const im_rect src_rect, const im_rect dst_rect, const im_rect pat_rect, const int mode_usage);
/* 兼容旧版符号 */
IM_C_API void rga_check_perpare(rga_buffer_t *src, rga_buffer_t *dst, rga_buffer_t *pat,
                                im_rect *src_rect, im_rect *dst_rect, im_rect *pat_rect, int mode_usage);

/**
 * 阻塞等待所有执行完成
 *
 * @param release_fence_fd
 *      RGA job 完成释放的 fence fd
 *
 * @returns 成功返回 0，否则返回负错误码
 */
IM_EXPORT_API IM_STATUS imsync(int release_fence_fd);

/**
 * 配置 RGA 参数
 *
 * @param name
 *      配置项名称（IM_CONFIG_NAME 枚举，如调度器核心、优先级等）
 * @param value
 *      配置值
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_EXPORT_API IM_STATUS imconfig(IM_CONFIG_NAME name, uint64_t value);

#endif /* #ifndef _im2d_common_h_ */
