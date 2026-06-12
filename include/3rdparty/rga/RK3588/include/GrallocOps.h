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

#ifndef _rk_graphic_buffer_h_
#define _rk_graphic_buffer_h_

#ifdef ANDROID

#include <stdint.h>
#include <vector>
#include <sys/types.h>

#include <system/graphics.h>

#include <utils/Thread.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include <sys/mman.h>
#include <linux/stddef.h>

#include <utils/Atomic.h>
#include <utils/Errors.h>
#include <android/log.h>
#include <utils/Log.h>
#include <log/log_main.h>

#include "drmrga.h"
#include "rga.h"

// -------------------------------------------------------------------------------
// 以下为 Android Gralloc 缓冲区操作辅助函数，用于获取 GraphicBuffer 的文件描述符、属性和映射地址

/* 获取 GraphicBuffer 句柄对应的文件描述符（DMA-BUF fd） */
int         RkRgaGetHandleFd(buffer_handle_t handle, int *fd);
/* 获取 GraphicBuffer 句柄的属性列表（宽、高、格式、步长等） */
int         RkRgaGetHandleAttributes(buffer_handle_t handle,
                                     std::vector<int> *attrs);
/* 获取 GraphicBuffer 句柄映射到用户空间的虚拟地址 */
int         RkRgaGetHandleMapAddress(buffer_handle_t handle,
                                     void **buf);
#endif  //Android

#endif  //_rk_graphic_buffer_h_
