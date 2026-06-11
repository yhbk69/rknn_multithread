/****************************************************************************
*
*    Copyright (c) 2017 - 2022 by Rockchip Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Rockchip Corporation. This is proprietary information owned by
*    Rockchip Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Rockchip Corporation.
*
*****************************************************************************/


#ifndef _RKNN_API_H
#define _RKNN_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
    rknn_init 扩展标志定义
*/
/* 设置高优先级上下文 */
#define RKNN_FLAG_PRIOR_HIGH                    0x00000000

/* 设置中优先级上下文 */
#define RKNN_FLAG_PRIOR_MEDIUM                  0x00000001

/* 设置低优先级上下文 */
#define RKNN_FLAG_PRIOR_LOW                     0x00000002

/* 异步模式。
   启用后 rknn_outputs_get 不会长时间阻塞，因为它直接获取上一帧的结果，
   可以在单线程模式下提高帧率，但代价是 rknn_outputs_get 获取的不是当前帧的结果。
   在多线程模式下无需开启此模式。 */
#define RKNN_FLAG_ASYNC_MASK                    0x00000004

/* 性能采集模式。
   启用后可通过 rknn_query(ctx, RKNN_QUERY_PERF_DETAIL, ...) 获取详细性能报告，
   但会降低帧率。 */
#define RKNN_FLAG_COLLECT_PERF_MASK             0x00000008

/* 在外部分配所有内存，包括权重/内部/输入/输出 */
#define RKNN_FLAG_MEM_ALLOC_OUTSIDE             0x00000010

/* 相同网络结构间共享权重 */
#define RKNN_FLAG_SHARE_WEIGHT_MEM              0x00000020

/* 从外部传入 fence 文件描述符 */
#define RKNN_FLAG_FENCE_IN_OUTSIDE              0x00000040

/* 从内部获取 fence 文件描述符 */
#define RKNN_FLAG_FENCE_OUT_OUTSIDE             0x00000080

/* 虚拟初始化标志：仅可通过 rknn_query 获取权重总大小和内部内存总大小*/
#define RKNN_FLAG_COLLECT_MODEL_INFO_ONLY       0x00000100

/* 当算子不被 NPU 支持时，设置 GPU 为首选执行后端 */
#define RKNN_FLAG_EXECUTE_FALLBACK_PRIOR_DEVICE_GPU 0x00000400

/* 在外部分配内部内存 */
#define RKNN_FLAG_INTERNAL_ALLOC_OUTSIDE        0x00000200

/*
    RKNN API 返回的错误码
*/
#define RKNN_SUCC                               0       /* 执行成功 */
#define RKNN_ERR_FAIL                           -1      /* 执行失败 */
#define RKNN_ERR_TIMEOUT                        -2      /* 执行超时 */
#define RKNN_ERR_DEVICE_UNAVAILABLE             -3      /* 设备不可用 */
#define RKNN_ERR_MALLOC_FAIL                    -4      /* 内存分配失败 */
#define RKNN_ERR_PARAM_INVALID                  -5      /* 参数无效 */
#define RKNN_ERR_MODEL_INVALID                  -6      /* 模型无效 */
#define RKNN_ERR_CTX_INVALID                    -7      /* 上下文无效 */
#define RKNN_ERR_INPUT_INVALID                  -8      /* 输入无效 */
#define RKNN_ERR_OUTPUT_INVALID                 -9      /* 输出无效 */
#define RKNN_ERR_DEVICE_UNMATCH                 -10     /* 设备不匹配，请更新 rknn sdk
                                                           和 npu 驱动/固件 */
#define RKNN_ERR_INCOMPATILE_PRE_COMPILE_MODEL  -11     /* 此 RKNN 模型使用预编译模式，但与当前驱动不兼容 */
#define RKNN_ERR_INCOMPATILE_OPTIMIZATION_LEVEL_VERSION  -12     /* 此 RKNN 模型设置了优化等级，但与当前驱动不兼容 */
#define RKNN_ERR_TARGET_PLATFORM_UNMATCH        -13     /* 此 RKNN 模型设置了目标平台，但与当前平台不兼容 */

/*
    张量定义
*/
#define RKNN_MAX_DIMS                           16      /* 张量最大维度数 */
#define RKNN_MAX_NUM_CHANNEL                    15      /* 输入张量最大通道数 */
#define RKNN_MAX_NAME_LEN                       256     /* 张量名称最大长度 */
#define RKNN_MAX_DYNAMIC_SHAPE_NUM              512     /* 每个输入的最大动态形状数量 */

#ifdef __arm__
typedef uint32_t rknn_context;
#else
typedef uint64_t rknn_context;
#endif


/*
    rknn_query 查询命令
*/
typedef enum _rknn_query_cmd {
    RKNN_QUERY_IN_OUT_NUM = 0,                              /* 查询输入和输出张量的数量 */
    RKNN_QUERY_INPUT_ATTR = 1,                              /* 查询输入张量的属性 */
    RKNN_QUERY_OUTPUT_ATTR = 2,                             /* 查询输出张量的属性 */
    RKNN_QUERY_PERF_DETAIL = 3,                             /* 查询详细性能，需要在 rknn_init 调用时设置
                                                               RKNN_FLAG_COLLECT_PERF_MASK，
                                                               此查询需要在 rknn_outputs_get 之后才有效 */
    RKNN_QUERY_PERF_RUN = 4,                                /* 查询运行时间，
                                                               此查询需要在 rknn_outputs_get 之后才有效 */
    RKNN_QUERY_SDK_VERSION = 5,                             /* 查询 sdk 和驱动版本 */

    RKNN_QUERY_MEM_SIZE = 6,                                /* 查询权重和内部内存大小 */
    RKNN_QUERY_CUSTOM_STRING = 7,                           /* 查询自定义字符串 */

    RKNN_QUERY_NATIVE_INPUT_ATTR = 8,                       /* 查询原始输入张量的属性 */
    RKNN_QUERY_NATIVE_OUTPUT_ATTR = 9,                      /* 查询原始输出张量的属性 */

    RKNN_QUERY_NATIVE_NC1HWC2_INPUT_ATTR = 8,               /* 查询原始输入张量的属性 */
    RKNN_QUERY_NATIVE_NC1HWC2_OUTPUT_ATTR = 9,              /* 查询原始输出张量的属性 */

    RKNN_QUERY_NATIVE_NHWC_INPUT_ATTR = 10,                 /* 查询原始输入张量的属性 */
    RKNN_QUERY_NATIVE_NHWC_OUTPUT_ATTR = 11,                /* 查询原始输出张量的属性 */

    RKNN_QUERY_DEVICE_MEM_INFO = 12,                        /* 查询 rknn 内存信息属性 */

    RKNN_QUERY_INPUT_DYNAMIC_RANGE = 13,                    /* 查询 rknn 输入张量的动态形状范围 */
    RKNN_QUERY_CURRENT_INPUT_ATTR = 14,                     /* 查询 rknn 输入张量的当前形状，仅对动态 rknn 模型有效*/
    RKNN_QUERY_CURRENT_OUTPUT_ATTR = 15,                    /* 查询 rknn 输出张量的当前形状，仅对动态 rknn 模型有效*/

    RKNN_QUERY_CURRENT_NATIVE_INPUT_ATTR = 16,              /* 查询 rknn 输入张量的当前原始形状，仅对动态 rknn 模型有效*/
    RKNN_QUERY_CURRENT_NATIVE_OUTPUT_ATTR = 17,             /* 查询 rknn 输出张量的当前原始形状，仅对动态 rknn 模型有效*/


    RKNN_QUERY_CMD_MAX
} rknn_query_cmd;

/*
    张量数据类型
*/
typedef enum _rknn_tensor_type {
    RKNN_TENSOR_FLOAT32 = 0,                            /* 数据类型为 float32 */
    RKNN_TENSOR_FLOAT16,                                /* 数据类型为 float16 */
    RKNN_TENSOR_INT8,                                   /* 数据类型为 int8 */
    RKNN_TENSOR_UINT8,                                  /* 数据类型为 uint8 */
    RKNN_TENSOR_INT16,                                  /* 数据类型为 int16 */
    RKNN_TENSOR_UINT16,                                 /* 数据类型为 uint16 */
    RKNN_TENSOR_INT32,                                  /* 数据类型为 int32 */
    RKNN_TENSOR_UINT32,                                 /* 数据类型为 uint32 */
    RKNN_TENSOR_INT64,                                  /* 数据类型为 int64 */
    RKNN_TENSOR_BOOL,

    RKNN_TENSOR_TYPE_MAX
} rknn_tensor_type;

inline static const char* get_type_string(rknn_tensor_type type)
{
    switch(type) {
    case RKNN_TENSOR_FLOAT32: return "FP32";
    case RKNN_TENSOR_FLOAT16: return "FP16";
    case RKNN_TENSOR_INT8: return "INT8";
    case RKNN_TENSOR_UINT8: return "UINT8";
    case RKNN_TENSOR_INT16: return "INT16";
    case RKNN_TENSOR_UINT16: return "UINT16";
    case RKNN_TENSOR_INT32: return "INT32";
    case RKNN_TENSOR_UINT32: return "UINT32";
    case RKNN_TENSOR_INT64: return "INT64";
    case RKNN_TENSOR_BOOL: return "BOOL";
    default: return "UNKNOW";
    }
}

/*
    量化类型
*/
typedef enum _rknn_tensor_qnt_type {
    RKNN_TENSOR_QNT_NONE = 0,                           /* 无 */
    RKNN_TENSOR_QNT_DFP,                                /* 动态定点 */
    RKNN_TENSOR_QNT_AFFINE_ASYMMETRIC,                  /* 非对称仿射 */

    RKNN_TENSOR_QNT_MAX
} rknn_tensor_qnt_type;

inline static const char* get_qnt_type_string(rknn_tensor_qnt_type type)
{
    switch(type) {
    case RKNN_TENSOR_QNT_NONE: return "NONE";
    case RKNN_TENSOR_QNT_DFP: return "DFP";
    case RKNN_TENSOR_QNT_AFFINE_ASYMMETRIC: return "AFFINE";
    default: return "UNKNOW";
    }
}

/*
    张量数据格式
*/
typedef enum _rknn_tensor_format {
    RKNN_TENSOR_NCHW = 0,                               /* 数据格式为 NCHW */
    RKNN_TENSOR_NHWC,                                   /* 数据格式为 NHWC */
    RKNN_TENSOR_NC1HWC2,                                /* 数据格式为 NC1HWC2 */
    RKNN_TENSOR_UNDEFINED,

    RKNN_TENSOR_FORMAT_MAX
} rknn_tensor_format;

/*
    目标 NPU 核心运行模式
*/
typedef enum _rknn_core_mask {
    RKNN_NPU_CORE_AUTO = 0,                                       /* 默认，随机运行在 NPU 核心上 */
    RKNN_NPU_CORE_0 = 1,                                          /* 运行在 NPU 核心 0 上 */
    RKNN_NPU_CORE_1 = 2,                                          /* 运行在 NPU 核心 1 上 */
    RKNN_NPU_CORE_2 = 4,                                          /* 运行在 NPU 核心 2 上 */
    RKNN_NPU_CORE_0_1 = RKNN_NPU_CORE_0 | RKNN_NPU_CORE_1,        /* 运行在 NPU 核心 0 和核心 1 上 */
    RKNN_NPU_CORE_0_1_2 = RKNN_NPU_CORE_0_1 | RKNN_NPU_CORE_2,    /* 运行在 NPU 核心 0、核心 1 和核心 2 上 */

    RKNN_NPU_CORE_UNDEFINED,
} rknn_core_mask;

inline static const char* get_format_string(rknn_tensor_format fmt)
{
    switch(fmt) {
    case RKNN_TENSOR_NCHW: return "NCHW";
    case RKNN_TENSOR_NHWC: return "NHWC";
    case RKNN_TENSOR_NC1HWC2: return "NC1HWC2";
    case RKNN_TENSOR_UNDEFINED: return "UNDEFINED";
    default: return "UNKNOW";
    }
}

/*
    RKNN_QUERY_IN_OUT_NUM 的信息
*/
typedef struct _rknn_input_output_num {
    uint32_t n_input;                                   /* 输入数量 */
    uint32_t n_output;                                  /* 输出数量 */
} rknn_input_output_num;

/*
    RKNN_QUERY_INPUT_ATTR / RKNN_QUERY_OUTPUT_ATTR 的信息
*/
typedef struct _rknn_tensor_attr {
    uint32_t index;                                     /* 输入参数，输入/输出张量的索引，
                                                           需要在调用 rknn_query 前设置 */

    uint32_t n_dims;                                    /* 维度数量 */
    uint32_t dims[RKNN_MAX_DIMS];                       /* 维度数组 */
    char name[RKNN_MAX_NAME_LEN];                       /* 张量名称 */

    uint32_t n_elems;                                   /* 元素数量 */
    uint32_t size;                                      /* 张量的字节大小 */

    rknn_tensor_format fmt;                             /* 张量的数据格式 */
    rknn_tensor_type type;                              /* 张量的数据类型 */
    rknn_tensor_qnt_type qnt_type;                      /* 张量的量化类型 */
    int8_t fl;                                          /* RKNN_TENSOR_QNT_DFP 的小数长度 */
    int32_t zp;                                         /* RKNN_TENSOR_QNT_AFFINE_ASYMMETRIC 的零点 */
    float scale;                                        /* RKNN_TENSOR_QNT_AFFINE_ASYMMETRIC 的缩放因子 */

    uint32_t w_stride;                                  /* 张量沿输入宽度维度的步长，
                                                           注意：此为只读，0 表示等于宽度 */
    uint32_t size_with_stride;                          /* 带步长的张量字节大小 */

    uint8_t pass_through;                               /* 直通模式，用于 rknn_set_io_mem 接口。
                                                           如果为 TRUE，buf 数据直接传递给 rknn 模型的输入节点
                                                                    无需任何转换。以下变量无需设置。
                                                           如果为 FALSE，buf 数据将按照以下 type 和 fmt 转换为与模型
                                                                     一致的输入。因此以下变量需要设置。*/
    uint32_t h_stride;                                  /* 沿输入高度维度的步长，
                                                           注意：此为只写，如果设置为 0，h_stride = height。 */
} rknn_tensor_attr;

typedef struct _rknn_input_range {
    uint32_t index;                                                 /* 输入参数，输入/输出张量的索引，
                                                                        需要在调用 rknn_query 前设置 */
    uint32_t shape_number;                                          /* 形状数量 */
    rknn_tensor_format fmt;                                         /* 张量的数据格式 */
    char name[RKNN_MAX_NAME_LEN];                                   /* 张量名称 */
    uint32_t dyn_range[RKNN_MAX_DYNAMIC_SHAPE_NUM][RKNN_MAX_DIMS];  /* 动态输入维度范围 */
    uint32_t n_dims;                                                /* 维度数量 */

} rknn_input_range;

/*
    RKNN_QUERY_PERF_DETAIL 的信息
*/
typedef struct _rknn_perf_detail {
    char* perf_data;                                    /* 性能详情的字符串指针，用户无需释放 */
    uint64_t data_len;                                  /* 字符串长度 */
} rknn_perf_detail;

/*
    RKNN_QUERY_PERF_RUN 的信息
*/
typedef struct _rknn_perf_run {
    int64_t run_duration;                               /* 实际推理时间（微秒） */
} rknn_perf_run;

/*
    RKNN_QUERY_SDK_VERSION 的信息
*/
typedef struct _rknn_sdk_version {
    char api_version[256];                              /* rknn api 的版本 */
    char drv_version[256];                              /* rknn 驱动的版本 */
} rknn_sdk_version;

/*
    RKNN_QUERY_MEM_SIZE 的信息
*/
typedef struct _rknn_mem_size {
    uint32_t total_weight_size;                         /* 权重内存大小 */
    uint32_t total_internal_size;                       /* 内部内存大小，不包括输入/输出 */
    uint64_t total_dma_allocated_size;                  /* DMA 内存分配总大小 */
    uint32_t total_sram_size;                           /* 为 rknn 保留的系统 sram 总大小 */
    uint32_t free_sram_size;                            /* 为 rknn 保留的系统 sram 空闲大小 */
    uint32_t reserved[10];                              /* 保留 */
} rknn_mem_size;

/*
    RKNN_QUERY_CUSTOM_STRING 的信息
*/
typedef struct _rknn_custom_string {
    char string[1024];                                  /* 自定义字符串，最大长度 1024 字节 */
} rknn_custom_string;

/*
   rknn_tensor_mem 的标志
*/
typedef enum _rknn_tensor_mem_flags {
    RKNN_TENSOR_MEMORY_FLAGS_ALLOC_INSIDE = 1,           /*用于在 rknn_destroy_mem() 中标记是否需要释放 "mem" 指针本身。
                                                         如果设置了 RKNN_TENSOR_MEMORY_FLAGS_ALLOC_INSIDE 标志，rknn_destroy_mem() 会调用 free(mem)。*/
    RKNN_TENSOR_MEMORY_FLAGS_FROM_FD      = 2,           /*用于在 rknn_create_mem_from_fd() 中标记是否需要释放 "mem" 指针本身。
                                                         如果设置了 RKNN_TENSOR_MEMORY_FLAGS_FROM_FD 标志，rknn_destroy_mem() 会调用 free(mem)。*/
    RKNN_TENSOR_MEMORY_FLAGS_FROM_PHYS    = 3,           /*用于在 rknn_create_mem_from_phys() 中标记是否需要释放 "mem" 指针本身。
                                                         如果设置了 RKNN_TENSOR_MEMORY_FLAGS_FROM_PHYS 标志，rknn_destroy_mem() 会调用 free(mem)。*/
    RKNN_TENSOR_MEMORY_FLAGS_UNKNOWN
} rknn_tensor_mem_flags;

/*
    张量的内存信息
*/
typedef struct _rknn_tensor_memory {
    void*            virt_addr;                         /* 张量缓冲区的虚拟地址 */
    uint64_t         phys_addr;                         /* 张量缓冲区的物理地址 */
    int32_t          fd;                                /* 张量缓冲区的文件描述符 */
    int32_t          offset;                            /* 表示内存的偏移量 */
    uint32_t         size;                              /* 张量缓冲区的大小 */
    uint32_t         flags;                             /* 张量缓冲区的标志，保留 */
    void *           priv_data;                         /* 张量缓冲区的私有数据 */
} rknn_tensor_mem;

/*
    rknn_input_set 的输入信息
*/
typedef struct _rknn_input {
    uint32_t index;                                     /* 输入索引 */
    void* buf;                                          /* 索引对应的输入缓冲区 */
    uint32_t size;                                      /* 输入缓冲区的大小 */
    uint8_t pass_through;                               /* 直通模式。
                                                           如果为 TRUE，buf 数据直接传递给 rknn 模型的输入节点
                                                                    无需任何转换。以下变量无需设置。
                                                           如果为 FALSE，buf 数据将按照以下 type 和 fmt 转换为与模型
                                                                     一致的输入。因此以下变量需要设置。*/
    rknn_tensor_type type;                              /* 输入缓冲区的数据类型 */
    rknn_tensor_format fmt;                             /* 输入缓冲区的数据格式。
                                                           目前 NPU 的内部输入格式默认为 NCHW。
                                                           因此输入 NCHW 数据可以避免驱动中的格式转换。 */
} rknn_input;

/*
    rknn_outputs_get 的输出信息
*/
typedef struct _rknn_output {
    uint8_t want_float;                                 /* 是否希望将输出数据转换为 float */
    uint8_t is_prealloc;                                /* buf 是否预分配。
                                                           如果为 TRUE，以下变量需要设置。
                                                           如果为 FALSE，以下变量无需设置。 */
    uint32_t index;                                     /* 输出索引 */
    void* buf;                                          /* 索引对应的输出缓冲区。
                                                           当 is_prealloc = FALSE 且调用了 rknn_outputs_release 时，
                                                           此 buf 指针将被释放，不可再使用。 */
    uint32_t size;                                      /* 输出缓冲区的大小 */
} rknn_output;

/*
    rknn_init 的扩展信息
*/
typedef struct _rknn_init_extend {
    rknn_context ctx;                                    /* rknn 上下文 */
    int32_t      real_model_offset;                      /* 真实 rknn 模型文件偏移，仅在使用 rknn 文件路径初始化上下文时有效 */
    uint32_t     real_model_size;                        /* 真实 rknn 模型文件大小，仅在使用 rknn 文件路径初始化上下文时有效 */
    uint8_t      reserved[120];                          /* 保留 */
} rknn_init_extend;

/*
    rknn_run 的扩展信息
*/
typedef struct _rknn_run_extend {
    uint64_t frame_id;                                  /* 输出参数，指示当前运行的帧 id */
    int32_t non_block;                                  /* 运行的阻塞标志，0 为阻塞，1 为非阻塞 */
    int32_t timeout_ms;                                 /* 阻塞模式的超时时间，单位为毫秒 */
    int32_t fence_fd;                                   /* 来自其他单元的 fence 文件描述符 */
} rknn_run_extend;

/*
    rknn_outputs_get 的扩展信息
*/
typedef struct _rknn_output_extend {
    uint64_t frame_id;                                  /* 输出参数，指示输出的帧 id，对应
                                                           struct rknn_run_extend.frame_id。*/
} rknn_output_extend;


/*  rknn_init

    初始化上下文并加载 rknn 模型。

    输入:
        rknn_context* context       上下文句柄的指针。
        void* model                 如果 size > 0，指向 rknn 模型的指针；如果 size = 0，rknn 模型的文件路径。
        uint32_t size               rknn 模型的大小。
        uint32_t flag               扩展标志，参见 RKNN_FLAG_XXX_XXX 的定义。
        rknn_init_extend* extend    初始化的扩展信息。
    返回:
        int                         错误码。
*/
int rknn_init(rknn_context* context, void* model, uint32_t size, uint32_t flag, rknn_init_extend* extend);

/*  rknn_dup_context

    初始化上下文并加载 rknn 模型。

    输入:
        rknn_context* context_in       输入上下文句柄的指针。
        rknn_context* context_out      输出上下文句柄的指针。
    返回:
        int                         错误码。
*/
int rknn_dup_context(rknn_context* context_in, rknn_context* context_out);

/*  rknn_destroy

    卸载 rknn 模型并销毁上下文。

    输入:
        rknn_context context        上下文句柄。
    返回:
        int                         错误码。
*/
int rknn_destroy(rknn_context context);


/*  rknn_query

    查询模型或其他相关信息。参见 rknn_query_cmd。

    输入:
        rknn_context context        上下文句柄。
        rknn_query_cmd cmd          查询命令。
        void* info                  信息缓冲区指针。
        uint32_t size               信息的大小。
    返回:
        int                         错误码。
*/
int rknn_query(rknn_context context, rknn_query_cmd cmd, void* info, uint32_t size);


/*  rknn_inputs_set

    通过 rknn 模型的输入索引设置输入信息。
    输入信息参见 rknn_input。

    输入:
        rknn_context context        上下文句柄。
        uint32_t n_inputs           输入的数量。
        rknn_input inputs[]         输入信息的数组，参见 rknn_input。
    返回:
        int                         错误码
*/
int rknn_inputs_set(rknn_context context, uint32_t n_inputs, rknn_input inputs[]);

/*
    rknn_set_batch_core_num

    设置 rknn 批处理核心数量。

    输入:
        rknn_context context        上下文句柄。
        int core_num                核心数量。
    返回:
        int                         错误码。

*/
int rknn_set_batch_core_num(rknn_context context, int core_num);

/*  rknn_set_core_mask

    设置 rknn 核心掩码。（目前仅 RK3588 支持）

    RKNN_NPU_CORE_AUTO: 自动模式，默认值
    RKNN_NPU_CORE_0: 核心 0 模式
    RKNN_NPU_CORE_1: 核心 1 模式
    RKNN_NPU_CORE_2: 核心 2 模式
    RKNN_NPU_CORE_0_1: 组合核心 0/1 模式
    RKNN_NPU_CORE_0_1_2: 组合核心 0/1/2 模式

    输入:
        rknn_context context        上下文句柄。
        rknn_core_mask core_mask    核心掩码。
    返回:
        int                         错误码。
*/
int rknn_set_core_mask(rknn_context context, rknn_core_mask core_mask);

/*  rknn_run

    运行模型执行推理。

    输入:
        rknn_context context        上下文句柄。
        rknn_run_extend* extend     运行的扩展信息。
    返回:
        int                         错误码。
*/
int rknn_run(rknn_context context, rknn_run_extend* extend);


/*  rknn_wait

    等待模型执行推理完成。

    输入:
        rknn_context context        上下文句柄。
        rknn_run_extend* extend     运行的扩展信息。
    返回:
        int                         错误码。
*/
int rknn_wait(rknn_context context, rknn_run_extend* extend);


/*  rknn_outputs_get

    等待推理完成并获取输出。
    此函数将阻塞直到推理完成。
    结果将设置到 outputs[] 中。

    输入:
        rknn_context context        上下文句柄。
        uint32_t n_outputs          输出的数量。
        rknn_output outputs[]       输出数组，参见 rknn_output。
        rknn_output_extend*         输出的扩展信息。
    返回:
        int                         错误码。
*/
int rknn_outputs_get(rknn_context context, uint32_t n_outputs, rknn_output outputs[], rknn_output_extend* extend);


/*  rknn_outputs_release

    释放通过 rknn_outputs_get 获取的输出。
    调用后，当 rknn_output[x].is_prealloc = FALSE 时，
    通过 rknn_outputs_get 获取的 rknn_output[x].buf 也将被释放。

    输入:
        rknn_context context        上下文句柄。
        uint32_t n_ouputs           输出的数量。
        rknn_output outputs[]       输出数组。
    返回:
        int                         错误码
*/
int rknn_outputs_release(rknn_context context, uint32_t n_ouputs, rknn_output outputs[]);


/* 零拷贝的新接口 */

/*  rknn_create_mem_from_phys（内存在外部分配）

    从物理地址初始化张量内存。

    输入:
        rknn_context ctx            上下文句柄。
        uint64_t phys_addr          物理地址。
        void *virt_addr             虚拟地址。
        uint32_t size               张量缓冲区的大小。
    返回:
        rknn_tensor_mem             张量内存信息的指针。
*/
rknn_tensor_mem* rknn_create_mem_from_phys(rknn_context ctx, uint64_t phys_addr, void *virt_addr, uint32_t size);


/*  rknn_create_mem_from_fd（内存在外部分配）

    从文件描述符初始化张量内存。

    输入:
        rknn_context ctx            上下文句柄。
        int32_t fd                  文件描述符。
        void *virt_addr             虚拟地址。
        uint32_t size               张量缓冲区的大小。
        int32_t offset              表示内存的偏移量（virt_addr 不含偏移）。
    返回:
        rknn_tensor_mem             张量内存信息的指针。
*/
rknn_tensor_mem* rknn_create_mem_from_fd(rknn_context ctx, int32_t fd, void *virt_addr, uint32_t size, int32_t offset);


/*  rknn_create_mem_from_mb_blk（内存在外部分配）

    从 mb_blk 创建张量内存。

    输入:
        rknn_context ctx            上下文句柄。
        void *mb_blk                从系统 api 分配的 mb_blk。
        int32_t offset              表示内存的偏移量。
    返回:
        rknn_tensor_mem             张量内存信息的指针。
*/
rknn_tensor_mem* rknn_create_mem_from_mb_blk(rknn_context ctx, void *mb_blk, int32_t offset);


/*  rknn_create_mem（内存在内部分配）

    创建张量内存。

    输入:
        rknn_context ctx            上下文句柄。
        uint32_t size               张量缓冲区的大小。
    返回:
        rknn_tensor_mem             张量内存信息的指针。
*/
rknn_tensor_mem* rknn_create_mem(rknn_context ctx, uint32_t size);


/*  rknn_destroy_mem（支持内部和外部分配）

    销毁张量内存。

    输入:
        rknn_context ctx            上下文句柄。
        rknn_tensor_mem *mem        张量内存信息的指针。
    返回:
        int                         错误码
*/
int rknn_destroy_mem(rknn_context ctx, rknn_tensor_mem *mem);


/*  rknn_set_weight_mem

    设置权重内存。

    输入:
        rknn_context ctx            上下文句柄。
        rknn_tensor_mem *mem        张量内存信息的数组
    返回:
        int                         错误码。
*/
int rknn_set_weight_mem(rknn_context ctx, rknn_tensor_mem *mem);


/*  rknn_set_internal_mem

    设置内部内存。

    输入:
        rknn_context ctx            上下文句柄。
        rknn_tensor_mem *mem        张量内存信息的数组
    返回:
        int                         错误码。
*/
int rknn_set_internal_mem(rknn_context ctx, rknn_tensor_mem *mem);


/*  rknn_set_io_mem

    设置输入和输出张量的缓冲区。

    输入:
        rknn_context ctx            上下文句柄。
        rknn_tensor_mem *mem        张量内存信息的数组。
        rknn_tensor_attr *attr      输入或输出张量缓冲区的属性。
    返回:
        int                         错误码。
*/
int rknn_set_io_mem(rknn_context ctx, rknn_tensor_mem *mem, rknn_tensor_attr *attr);

/*  rknn_set_input_shape（已弃用）

    设置输入张量形状（仅对动态形状的 rknn 模型有效）。

    输入:
        rknn_context ctx            上下文句柄。
        rknn_tensor_attr *attr      输入或输出张量缓冲区的属性。
    返回:
        int                         错误码。
*/
int rknn_set_input_shape(rknn_context ctx, rknn_tensor_attr* attr);

/*  rknn_set_input_shapes

    设置所有输入张量的形状。调用 rknn_set_input_shapes 后，图将在当前设置的输入形状下运行。（仅对动态形状的 rknn 模型有效）。

    输入:
        rknn_context ctx            上下文句柄。
        uint32_t n_inputs           输入的数量。
        rknn_tensor_attr attr[]     所有输入张量的属性数组。
    返回:
        int                         错误码。
*/
int rknn_set_input_shapes(rknn_context ctx, uint32_t n_inputs, rknn_tensor_attr attr[]);

#ifdef __cplusplus
} //extern "C"
#endif

#endif  //_RKNN_API_H
