// drm_func.h - DRM 内存管理相关函数声明
// 来自 Rockchip DRM 驱动接口，提供 DRM 设备初始化、缓冲区分配与释放等功能
#ifndef __DRM_FUNC_H__
#define __DRM_FUNC_H__
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>// open 系统调用
#include <unistd.h> // close 系统调用
#include <errno.h>
#include <sys/mman.h>


#include <linux/input.h>
#include "libdrm/drm_fourcc.h"
#include "xf86drm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (* FUNC_DRM_IOCTL)(int fd, unsigned long request, void *arg);

/* DRM 上下文结构体，保存 DRM 句柄和 IO 控制函数指针 */
typedef struct _drm_context{
    void *drm_handle;
    FUNC_DRM_IOCTL io_func;
} drm_context;

/* 内存类型定义 */
enum drm_rockchip_gem_mem_type
{
    /* 物理连续内存，默认使用此类型 */
    ROCKCHIP_BO_CONTIG = 1 << 0,
    /* 可缓存映射 */
    ROCKCHIP_BO_CACHABLE = 1 << 1,
    /* 写合并映射 */
    ROCKCHIP_BO_WC = 1 << 2,
    ROCKCHIP_BO_SECURE = 1 << 3,
    ROCKCHIP_BO_MASK = ROCKCHIP_BO_CONTIG | ROCKCHIP_BO_CACHABLE |
                       ROCKCHIP_BO_WC | ROCKCHIP_BO_SECURE
};

/* 初始化 DRM 设备，返回文件描述符 */
int drm_init(drm_context *drm_ctx);

/* 分配 DRM 缓冲区，返回虚拟地址指针 */
void* drm_buf_alloc(drm_context *drm_ctx,int drm_fd, int TexWidth, int TexHeight,int bpp,int *fd,unsigned int *handle,size_t *actual_size);

/* 销毁 DRM 缓冲区，释放相关内存 */
int drm_buf_destroy(drm_context *drm_ctx,int drm_fd,int buf_fd, int handle,void *drm_buf,size_t size);

/* 反初始化 DRM 设备，关闭文件描述符 */
void drm_deinit(drm_context *drm_ctx, int drm_fd);

#ifdef __cplusplus
}
#endif
#endif /*__DRM_FUNC_H__*/