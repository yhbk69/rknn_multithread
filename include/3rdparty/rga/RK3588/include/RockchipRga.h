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

#ifndef _rockchip_rga_h_
#define _rockchip_rga_h_

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/stddef.h>

#include "drmrga.h"
#include "GrallocOps.h"
#include "RgaUtils.h"
#include "rga.h"

//////////////////////////////////////////////////////////////////////////////////
#ifndef ANDROID
#include "RgaSingleton.h"
#endif

#ifdef ANDROID
#include <utils/Singleton.h>
#include <utils/Thread.h>
#include <hardware/hardware.h>

namespace android {
#endif

    /*
     * RockchipRga - RGA 硬件加速主操作类（单例模式）
     *
     * 封装了 RGA 设备的初始化/反初始化、缓冲区分配/释放/映射、
     * 图像合成（Blit）、颜色填充、调色板操作等核心功能。
     * 同时支持 Android（GraphicBuffer）和 Linux（DRM DMA-BUF）两种平台。
     */
    class RockchipRga :public Singleton<RockchipRga> {
      public:

        static inline RockchipRga& get() {
            return getInstance();
        }

        /* 初始化 RGA 设备 */
        int         RkRgaInit();
        /* 反初始化 RGA 设备，释放资源 */
        void        RkRgaDeInit();
        /* 获取 RGA 内部上下文指针 */
        void        RkRgaGetContext(void **ctx);
#ifndef ANDROID /* LINUX */
        /* 分配 DRM 缓冲区（需传入 drm_fd） */
        int         RkRgaAllocBuffer(int drm_fd /* input */, bo_t *bo_info,
                                     int width, int height, int bpp, int flags);
        /* 释放 DRM 缓冲区 */
        int         RkRgaFreeBuffer(int drm_fd /* input */, bo_t *bo_info);
        /* 分配缓冲区（简化接口） */
        int         RkRgaGetAllocBuffer(bo_t *bo_info, int width, int height, int bpp);
        /* 分配缓冲区（扩展接口，支持 flags） */
        int         RkRgaGetAllocBufferExt(bo_t *bo_info, int width, int height, int bpp, int flags);
        /* 分配带缓存属性的缓冲区 */
        int         RkRgaGetAllocBufferCache(bo_t *bo_info, int width, int height, int bpp);
        /* 将 DRM 缓冲区映射到用户空间 */
        int         RkRgaGetMmap(bo_t *bo_info);
        /* 解除 DRM 缓冲区的用户空间映射 */
        int         RkRgaUnmap(bo_t *bo_info);
        /* 释放缓冲区资源 */
        int         RkRgaFree(bo_t *bo_info);
        /* 获取缓冲区的 DMA-BUF 文件描述符 */
        int         RkRgaGetBufferFd(bo_t *bo_info, int *fd);
#else
        /* 获取 Android GraphicBuffer 的文件描述符 */
        int         RkRgaGetBufferFd(buffer_handle_t handle, int *fd);
        /* 获取 Android GraphicBuffer 映射到 CPU 的虚拟地址 */
        int         RkRgaGetHandleMapCpuAddress(buffer_handle_t handle, void **buf);
#endif
        /* 执行 RGA 图像合成操作（SVT + RGN → DST） */
        int         RkRgaBlit(rga_info *src, rga_info *dst, rga_info *src1);
        /* 执行颜色填充操作 */
        int         RkRgaCollorFill(rga_info *dst);
        /* 执行调色板操作（LUT 查表变换） */
        int         RkRgaCollorPalette(rga_info *src, rga_info *dst, rga_info *lut);
        /* 刷新 RGA 操作队列，确保所有提交的操作完成 */
        int         RkRgaFlush();

        /* 设置日志一次性输出标志 */
        void        RkRgaSetLogOnceFlag(int log) {
            mLogOnce = log;
        }
        /* 设置是否始终输出日志 */
        void        RkRgaSetAlwaysLogFlag(bool log) {
            mLogAlways = log;
        }
        /* 输出 RGA 寄存器请求内容到日志（调试用） */
        void        RkRgaLogOutRgaReq(struct rga_req rgaReg);
        /* 输出 RGA 用户参数内容到日志（调试用） */
        int         RkRgaLogOutUserPara(rga_info *rgaInfo);
        /* 检查 RGA 设备是否就绪可用 */
        inline bool RkRgaIsReady() {
            return mSupportRga;
        }

        RockchipRga();
        ~RockchipRga();
      private:
        bool                            mSupportRga;   /* RGA 硬件是否支持 */
        int                             mLogOnce;      /* 一次性日志标志 */
        int                             mLogAlways;    /* 持续日志标志 */
        void *                          mContext;      /* RGA 内部上下文 */

        friend class Singleton<RockchipRga>;
    };

#ifdef ANDROID
}; // namespace android
#endif

#endif

