/****************************************************************************
 *
 *    Copyright (c) 2017 - 2018 by Rockchip Corp.  All rights reserved.
 *
 *    The material in this file is confidential and contains trade secrets
 *    of Rockchip Corporation. This is proprietary information owned by
 *    Rockchip Corporation. No part of this work may be disclosed,
 *    reproduced, copied, transmitted, or used in any way for any purpose,
 *    without the express written permission of Rockchip Corporation.
 *
 *****************************************************************************/

#ifndef _RKNN_MATMUL_API_H
#define _RKNN_MATMUL_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rknn_api.h"

typedef rknn_context rknn_matmul_ctx;

typedef struct _rknn_matmul_tensor_attr
{
  char name[RKNN_MAX_NAME_LEN];

  // 标识矩阵 A(M, K) 或 B(K, N) 或 C(M, N)
  uint32_t n_dims;
  uint32_t dims[RKNN_MAX_DIMS];

  // 矩阵乘法张量大小
  uint32_t size;

  // 矩阵乘法张量数据类型
  // int8: 用于矩阵 A 和 B
  // int32: 用于矩阵 C
  rknn_tensor_type type;
} rknn_matmul_tensor_attr;

typedef struct _rknn_matmul_io_attr
{
  // 标识矩阵 A(M, K) 或 B(K, N) 或 C(M, N)
  rknn_matmul_tensor_attr A;
  rknn_matmul_tensor_attr B;
  rknn_matmul_tensor_attr C;
} rknn_matmul_io_attr;

/*
  矩阵乘法信息结构体
 */
typedef struct rknn_matmul_info_t
{
  int32_t M;
  int32_t K; // 限制：rk356x: int8 类型必须 32 字节对齐，float16 类型必须 16 字节对齐；
             // rk3588: int8 类型必须 32 字节对齐，float16 类型必须 32 字节对齐；
  int32_t N; // 限制：rk356x: int8 类型必须 16 字节对齐，float16 类型必须 8 字节对齐；
             // rk3588: int8 类型必须 32 字节对齐，float16 类型必须 16 字节对齐；

  // 矩阵乘法数据类型
  // int8: int8(A) x int8(B) -> int32(C)
  // float16: float16(A) x float16(B) -> float32(C)
  rknn_tensor_type type;

  // 矩阵 B 的原生内存布局
  // 0: 普通布局
  // 1: 原生布局
  int32_t native_layout;

  // 矩阵 A 和 C 的性能优化布局
  // 0: 普通布局
  // 1: 性能优化布局
  int32_t perf_layout;
} rknn_matmul_info;

/*  rknn_matmul_create

    参数:
        rknn_matmul_ctx *ctx           上下文句柄。
        rknn_matmul_info *info         矩阵乘法信息。
        rknn_matmul_io_attr *io_attr   输入/输出属性
    返回:
        int                         错误码
*/
int rknn_matmul_create(rknn_matmul_ctx* ctx, rknn_matmul_info* info, rknn_matmul_io_attr* io_attr);

/* rknn_matmul_set_io_mem

    参数:
        rknn_matmul_ctx ctx            上下文句柄。
        rknn_tensor_mem *mem           张量内存信息的指针。
        rknn_matmul_tensor_attr *attr  输入或输出张量缓冲区的属性。
    返回:
        int                         错误码。

    公式:
      C = A * B,

    限制:
      K <= 4096
      K 限制: rk356x: int8 类型必须 32 字节对齐，float16 类型必须 16 字节对齐；
               rk3588: int8 类型必须 32 字节对齐，float16 类型必须 32 字节对齐；
      N 限制: rk356x: int8 类型必须 16 字节对齐，float16 类型必须 8 字节对齐；
               rk3588: int8 类型必须 32 字节对齐，float16 类型必须 16 字节对齐；

    A 形状: M x K
      普通布局: (M, K)
              [M1K1, M1K2, ..., M1Kk,
               M2K1, M2K2, ..., M2Kk,
               ...
               MmK1, MmK2, ..., MmKk]
      适用于 rk356x：
      int8:
      性能优化布局: (K / 8, M, 8)
              [K1M1, K2M1,  ..., K8M1,
               K9M2, K10M2, ..., K16M2,
               ...
               K(k-7)Mm, K(k-6)Mm, ..., KkMm]
      float16:
      性能优化布局: (K / 4, M, 4)
              [K1M1, K2M1,  ..., K4M1,
               K9M2, K10M2, ..., K8M2,
               ...
               K(k-3)Mm, K(k-2)Mm, ..., KkMm]
      适用于 rk3588：
      int8:
      性能优化布局: (K / 16, M, 16)
              [K1M1, K2M1,  ..., K16M1,
               K9M2, K10M2, ..., K32M2,
               ...
               K(k-15)Mm, K(k-14)Mm, ..., KkMm]
      float16:
      性能优化布局: (K / 8, M, 8)
              [K1M1, K2M1,  ..., K8M1,
               K9M2, K10M2, ..., K16M2,
               ...
               K(k-7)Mm, K(k-6)Mm, ..., KkMm]
    B 形状: K x N
      普通布局: (K, N)
              [K1N1, K1N2, ..., K1Nn,
               K2N1, K2N2, ..., K2Nn,
               ...
               KkN1, KkN2, ..., KkNn]
      适用于 rk356x：
      int8:
      原生布局: (N / 16, K / 32, 16, 32)
              [K1N1,  K2N1,  ..., K32N1,
               K1N2,  K2N2,  ..., K32N2,
               ...
               K1N16, K2N16, ..., K32N16,
               K33N1, K34N1, ..., K64N1,
               K33N2, K34N2, ..., K64N2,
               ...
               K(k-31)N16, K(k-30)N16, ..., KkN16,
               K1N17, K2N17, ..., K32N17,
               K1N18, K2N18, ..., K32N18,
               ...
               K(k-31)Nn, K(k-30)Nn, ..., KkNn]
      float16:
      原生布局: (N / 8, K / 16, 8, 16)
              [K1N1,  K2N1,  ..., K16N1,
               K1N2,  K2N2,  ..., K16N2,
               ...
               K1N8,  K2N8,  ..., K16N8,
               K17N1, K18N1, ..., K32N1,
               K17N2, K18N2, ..., K32N2,
               ...
               K(k-15)N8, K(k-30)N8, ..., KkN8,
               K1N9,  K2N9,  ..., K16N9,
               K1N10, K2N10, ..., K16N10,
               ...
               K(k-15)Nn, K(k-14)Nn, ..., KkNn]
      适用于 rk3588：
      int8:
      原生布局: (N / 32, K / 32, 32, 32)
              [K1N1,  K2N1,  ..., K32N1,
               K1N2,  K2N2,  ..., K32N2,
               ...
               K1N32, K2N32, ..., K32N32,
               K33N1, K34N1, ..., K64N1,
               K33N2, K34N2, ..., K64N2,
               ...
               K(k-31)N32, K(k-30)N32, ..., KkN32,
               K1N33, K2N33, ..., K32N33,
               K1N34, K2N34, ..., K32N34,
               ...
               K(k-31)Nn, K(k-30)Nn, ..., KkNn]
      float16:
      原生布局: (N / 16, K / 32, 16, 32)
              [K1N1,  K2N1,  ..., K32N1,
               K1N2,  K2N2,  ..., K32N2,
               ...
               K1N16, K2N16, ..., K32N16,
               K33N1, K34N1, ..., K64N1,
               K33N2, K34N2, ..., K64N2,
               ...
               K(k-31)N16, K(k-30)N16, ..., KkN16,
               K1N17, K2N17, ..., K32N17,
               K1N18, K2N18, ..., K32N18,
               ...
               K(k-31)Nn, K(k-30)Nn, ..., KkNn]
    C 形状: M x N
      普通布局: (M, N)
              [M1N1, M1N2, ..., M1Nn,
               M2N1, M2N2, ..., M2Nn,
               ...
               MmN1, MmN2, ..., MmNn]
      性能优化布局: (N / 4, M, 4)
              [N1M1, N2M1, ..., N4M1,
               N5M2, N6M2, ..., N8M2,
               ...
               N(n-3)Mm, N(n-2)Mm, ..., NnMm]
 */
int rknn_matmul_set_io_mem(rknn_matmul_ctx ctx, rknn_tensor_mem* mem, rknn_matmul_tensor_attr* attr);

/*  rknn_matmul_set_core_mask

    设置 rknn 核心掩码。（当前仅支持 rk3588）

    RKNN_NPU_CORE_AUTO: 自动模式，默认值
    RKNN_NPU_CORE_0: 核心 0 模式
    RKNN_NPU_CORE_1: 核心 1 模式
    RKNN_NPU_CORE_2: 核心 2 模式
    RKNN_NPU_CORE_0_1: 组合核心 0/1 模式
    RKNN_NPU_CORE_0_1_2: 组合核心 0/1/2 模式

    输入:
        rknn_matmul_ctx context     上下文句柄。
        rknn_core_mask core_mask    核心掩码。
    返回:
        int                         错误码。
*/
int rknn_matmul_set_core_mask(rknn_matmul_ctx context, rknn_core_mask core_mask);

/*  rknn_matmul_run

    以阻塞模式执行矩阵乘法

    参数:
        rknn_matmul_ctx ctx         上下文句柄。
    返回:
        int                         错误码。
 */
int rknn_matmul_run(rknn_matmul_ctx ctx);

/*  rknn_matmul_destroy

    销毁矩阵乘法上下文

    参数:
        rknn_matmul_ctx ctx         上下文句柄。
    返回:
        int                         错误码。
 */
int rknn_matmul_destroy(rknn_matmul_ctx ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _RKNN_MATMUL_API_H