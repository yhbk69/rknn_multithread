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
#ifndef _im2d_mpi_hpp_
#define _im2d_mpi_hpp_

#include "im2d_type.h"

/**
 * 创建并配置 rockit-ko 的 RGA 上下文
 *
 * @param flags
 *      上下文配置标志位
 *
 * @returns 上下文 ID
 */
IM_EXPORT_API im_ctx_id_t imbegin(uint32_t flags);

/**
 * 取消并删除 rockit-ko 的 RGA 上下文
 *
 * @param id
 *      上下文 ID
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
IM_EXPORT_API IM_STATUS imcancel(im_ctx_id_t id);

/**
 * rockit-ko 模式的图像处理操作
 *
 * @param src               输入源图像（混合操作中作为背景）
 * @param dst               输出目标图像（混合操作中作为背景）
 * @param pat               前景图像或 LUT 查找表
 * @param srect             src 通道上需要处理的矩形区域
 * @param drect             dst 通道上需要处理的矩形区域
 * @param prect             pat 通道上需要处理的矩形区域
 * @param acquire_fence_fd  输入同步 fence fd（确保输入缓冲区就绪）
 * @param release_fence_fd  输出同步 fence fd（通知下游操作完成）
 * @param opt               图像处理选项配置
 * @param usage             图像处理模式标志
 * @param ctx_id            上下文 ID
 *
 * @returns 成功返回 IM_STATUS_SUCCESS，否则返回负错误码
 */
#ifdef __cplusplus
IM_API IM_STATUS improcess(rga_buffer_t src, rga_buffer_t dst, rga_buffer_t pat,
                           im_rect srect, im_rect drect, im_rect prect,
                           int acquire_fence_fd, int *release_fence_fd,
                           im_opt_t *opt, int usage, im_ctx_id_t ctx_id);
#endif
IM_EXPORT_API IM_STATUS improcess_ctx(rga_buffer_t src, rga_buffer_t dst, rga_buffer_t pat,
                                      im_rect srect, im_rect drect, im_rect prect,
                                      int acquire_fence_fd, int *release_fence_fd,
                                      im_opt_t *opt, int usage, im_ctx_id_t ctx_id);

#endif /* #ifndef _im2d_mpi_hpp_ */