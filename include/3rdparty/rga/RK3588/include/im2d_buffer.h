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
#ifndef _im2d_buffer_h_
#define _im2d_buffer_h_

#include "im2d_type.h"

/**
 * 将外部缓冲区导入 RGA 驱动（按大小模式）
 *
 * @param fd/va/pa
 *      根据缓冲区类型选择 dma_fd / 虚拟地址 / 物理地址
 * @param size
 *      图像缓冲区的大小
 *
 * @return rga_buffer_handle_t
 */
#ifdef __cplusplus
IM_API rga_buffer_handle_t importbuffer_fd(int fd, int size);
IM_API rga_buffer_handle_t importbuffer_virtualaddr(void *va, int size);
IM_API rga_buffer_handle_t importbuffer_physicaladdr(uint64_t pa, int size);
#endif

/**
 * 将外部缓冲区导入 RGA 驱动（按宽高格式模式）
 *
 * @param fd/va/pa
 *      根据缓冲区类型选择 dma_fd / 虚拟地址 / 物理地址
 * @param width
 *      图像缓冲区的像素宽度步长
 * @param height
 *      图像缓冲区的像素高度步长
 * @param format
 *      图像缓冲区的像素格式
 *
 * @return rga_buffer_handle_t
 */
#ifdef __cplusplus
IM_API rga_buffer_handle_t importbuffer_fd(int fd, int width, int height, int format);
IM_API rga_buffer_handle_t importbuffer_virtualaddr(void *va, int width, int height, int format);
IM_API rga_buffer_handle_t importbuffer_physicaladdr(uint64_t pa, int width, int height, int format);
#endif

/**
 * 将外部缓冲区导入 RGA 驱动（按参数结构体模式）
 *
 * @param fd/va/pa
 *      根据缓冲区类型选择 dma_fd / 虚拟地址 / 物理地址
 * @param param
 *      缓冲区参数配置
 *
 * @return rga_buffer_handle_t
 */
IM_EXPORT_API rga_buffer_handle_t importbuffer_fd(int fd, im_handle_param_t *param);
IM_EXPORT_API rga_buffer_handle_t importbuffer_virtualaddr(void *va, im_handle_param_t *param);
IM_EXPORT_API rga_buffer_handle_t importbuffer_physicaladdr(uint64_t pa, im_handle_param_t *param);

/**
 * 释放已导入的 RGA 缓冲区句柄
 *
 * @param handle
 *      RGA 缓冲区句柄
 *
 * @return 成功返回 0，否则返回负错误码
 */
IM_EXPORT_API IM_STATUS releasebuffer_handle(rga_buffer_handle_t handle);

/**
 * 包装图像参数为 rga_buffer_t 结构体
 *
 * @param handle/virtualaddr/physicaladdr/fd
 *      RGA 缓冲区句柄 / 虚拟地址 / 物理地址 / 文件描述符
 * @param width
 *      图像操作区域的宽度
 * @param height
 *      图像操作区域的高度
 * @param wstride
 *      宽度方向像素步长，默认与 width 相同
 * @param hstride
 *      高度方向像素步长，默认与 height 相同
 * @param format
 *      图像格式
 *
 * @return rga_buffer_t
 */
#define wrapbuffer_handle(handle, width, height, format, ...) \
    ({ \
        rga_buffer_t im2d_api_buffer; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            im2d_api_buffer = wrapbuffer_handle_t(handle, width, height, width, height, format); \
        } else if (__argc == 2){ \
            im2d_api_buffer = wrapbuffer_handle_t(handle, width, height, __args[0], __args[1], format); \
        } else { \
            memset(&im2d_api_buffer, 0x0, sizeof(im2d_api_buffer)); \
            printf("无效参数\n"); \
        } \
        im2d_api_buffer; \
    })

#define wrapbuffer_virtualaddr(vir_addr, width, height, format, ...) \
    ({ \
        rga_buffer_t im2d_api_buffer; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            im2d_api_buffer = wrapbuffer_virtualaddr_t(vir_addr, width, height, width, height, format); \
        } else if (__argc == 2){ \
            im2d_api_buffer = wrapbuffer_virtualaddr_t(vir_addr, width, height, __args[0], __args[1], format); \
        } else { \
            memset(&im2d_api_buffer, 0x0, sizeof(im2d_api_buffer)); \
            printf("无效参数\n"); \
        } \
        im2d_api_buffer; \
    })

#define wrapbuffer_physicaladdr(phy_addr, width, height, format, ...) \
    ({ \
        rga_buffer_t im2d_api_buffer; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            im2d_api_buffer = wrapbuffer_physicaladdr_t(phy_addr, width, height, width, height, format); \
        } else if (__argc == 2){ \
            im2d_api_buffer = wrapbuffer_physicaladdr_t(phy_addr, width, height, __args[0], __args[1], format); \
        } else { \
            memset(&im2d_api_buffer, 0x0, sizeof(im2d_api_buffer)); \
            printf("无效参数\n"); \
        } \
        im2d_api_buffer; \
    })

#define wrapbuffer_fd(fd, width, height, format, ...) \
    ({ \
        rga_buffer_t im2d_api_buffer; \
        int __args[] = {__VA_ARGS__}; \
        int __argc = sizeof(__args)/sizeof(int); \
        if (__argc == 0) { \
            im2d_api_buffer = wrapbuffer_fd_t(fd, width, height, width, height, format); \
        } else if (__argc == 2){ \
            im2d_api_buffer = wrapbuffer_fd_t(fd, width, height, __args[0], __args[1], format); \
        } else { \
            memset(&im2d_api_buffer, 0x0, sizeof(im2d_api_buffer)); \
            printf("无效参数\n"); \
        } \
        im2d_api_buffer; \
    })
/* 底层包装函数声明（由上方宏调用） */
IM_C_API rga_buffer_t wrapbuffer_handle_t(rga_buffer_handle_t handle, int width, int height, int wstride, int hstride, int format);
IM_C_API rga_buffer_t wrapbuffer_virtualaddr_t(void* vir_addr, int width, int height, int wstride, int hstride, int format);
IM_C_API rga_buffer_t wrapbuffer_physicaladdr_t(void* phy_addr, int width, int height, int wstride, int hstride, int format);
IM_C_API rga_buffer_t wrapbuffer_fd_t(int fd, int width, int height, int wstride, int hstride, int format);

#ifdef __cplusplus
#undef wrapbuffer_handle
IM_API rga_buffer_t wrapbuffer_handle(rga_buffer_handle_t  handle,
                                      int width, int height, int format);
IM_API rga_buffer_t wrapbuffer_handle(rga_buffer_handle_t  handle,
                                      int width, int height, int format,
                                      int wstride, int hstride);
#endif

#endif /* #ifndef _im2d_buffer_h_ */
