// rga_func.h - RGA 2D 图形加速相关函数声明
// 来自 Rockchip RGA 驱动接口，提供 RGA 模块初始化、图像缩放等功能
#ifndef __RGA_FUNC_H__
#define __RGA_FUNC_H__

#include <dlfcn.h> 
#include "RgaApi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RGA 函数指针类型定义 */
typedef int(* FUNC_RGA_INIT)();
typedef void(* FUNC_RGA_DEINIT)();
typedef int(* FUNC_RGA_BLIT)(rga_info_t *, rga_info_t *, rga_info_t *);

/* RGA 上下文结构体，保存动态加载的库句柄和函数指针 */
typedef struct _rga_context{
    void *rga_handle;
    FUNC_RGA_INIT init_func;
    FUNC_RGA_DEINIT deinit_func;
    FUNC_RGA_BLIT blit_func;
} rga_context;

/* 初始化 RGA 模块，动态加载 librga.so 并获取函数指针 */
int RGA_init(rga_context* rga_ctx);

/* 快速图像缩放，使用 DMA 文件描述符（物理地址），适用于硬件缓冲区 */
void img_resize_fast(rga_context *rga_ctx, int src_fd, int src_w, int src_h, uint64_t dst_phys, int dst_w, int dst_h);

/* 慢速图像缩放，使用虚拟地址指针，适用于普通内存缓冲区 */
void img_resize_slow(rga_context *rga_ctx, void *src_virt, int src_w, int src_h, void *dst_virt, int dst_w, int dst_h);

/* 反初始化 RGA 模块，关闭动态加载的库句柄 */
int RGA_deinit(rga_context* rga_ctx);

#ifdef __cplusplus
}
#endif
#endif/*__RGA_FUNC_H__*/
