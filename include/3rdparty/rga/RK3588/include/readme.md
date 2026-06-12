# RGA (Raster Graphics Acceleration) 头文件说明

## 概述

本目录包含 Rockchip RGA (librga) 库的 C/C++ 头文件，用于 RK3588 平台的 2D 图像硬件加速操作。RGA 是 Rockchip 平台专用的 2D 图形加速引擎，支持图像缩放、裁剪、旋转、翻转、色彩空间转换（YUV/RGB）、格式转换、Alpha 混合、色键抠图、OSD 叠加、马赛克、ROP 光栅操作、NN 量化、颜色填充、矩形绘制、调色板变换等功能。全部文件来源：**Rockchip SDK — librga 第三方库**。

---

## 头文件详细说明

### 核心 API 入口

| 文件 | 用途 |
|------|------|
| `im2d.h` | **im2d C 接口总入口头文件**。聚合引入 im2d_type、im2d_buffer、im2d_common、im2d_single、im2d_task、im2d_mpi 等子模块，是用户层最常用的统一引用入口。 |
| `im2d.hpp` | **im2d C++ 扩展入口头文件**。在 C 接口基础上额外引入 im2d_expand.h（Android 扩展预留），为 C++ 项目提供统一引用点。 |

### 类型与底层定义

| 文件 | 用途 |
|------|------|
| `im2d_type.h` | **im2d 类型定义**。定义所有核心数据类型：操作模式标志（IM_USAGE）、图像处理状态码（IM_STATUS）、矩形区域（im_rect）、色键范围（im_colorkey_range）、NN 量化配置（im_nn_t）、RGA 缓冲区（rga_buffer_t）、OSD 配置（im_osd_bpp2_t、im_osd_block_t、im_osd_invert_t）、操作选项（im_opt_t）等。 |
| `im2d_version.h` | **im2d API 版本号定义**。包含 RGA 库的主版本号、次版本号、修订号、构建号及版本字符串，提供 `querystring` 版本查询接口。 |
| `rga.h` | **RGA 像素格式与色彩空间定义**。定义 RGA 支持的所有像素格式枚举（RGBA8888、NV12、YUV420 等），以及 RGA2/RGA3 的渲染模式、旋转/翻转变换宏、色彩空间转换模式等。 |

### 缓冲区管理

| 文件 | 用途 |
|------|------|
| `im2d_buffer.h` | **im2d 缓冲区导入/包装接口**。提供从 DMA-BUF fd、虚拟地址、物理地址等多种来源导入外部缓冲区（importbuffer_* 系列），以及将现有句柄/地址/虚拟地址包装为 rga_buffer_t（wrapbuffer_* 系列）的函数和宏。 |

### 操作 API

| 文件 | 用途 |
|------|------|
| `im2d_single.h` | **im2d 单次操作 API（最常用）**。提供所有图像处理的单次同步/异步操作接口，包括：imcopy（复制）、imresize（缩放）、imcrop（裁剪）、imtranslate（平移）、imcvtcolor（格式转换）、imrotate（旋转）、imflip（翻转）、imblend（双通道混合）、imcomposite（三通道合成）、imcolorkey（色键抠图）、imosd（OSD 叠加）、imquantize（NN 量化）、imrop（ROP 操作）、imfill/imfillArray（颜色填充/批量填充）、imrectangle/imrectangleArray（矩形绘制/批量绘制）、immosaic/immosaicArray（马赛克/批量马赛克）、impalette（调色板）、improcess（复合处理）、immakeBorder（边框制作）。同时提供对应的 C 兼容宏接口。 |
| `im2d_task.h` | **im2d Job/Task 模式 API**。支持将多个操作封装到一个 Job 中批量提交执行（任务管线模式），提供 imbeginJob/imendJob/imcancelJob 等 Job 生命周期管理，以及所有操作对应的 *Task 版本函数（imcopyTask、imresizeTask 等），最后通过 imsubmitJob 一次性提交执行。 |
| `im2d_mpi.h` | **im2d 多进程接口**。提供基于 Job 模式的多进程支持，允许从一个进程创建 Job 并序列化后传递给另一个进程执行。包含 imbegin（开始 MPI 会话）、imcancel（取消 MPI 会话）、improcess（跨进程执行 Job）。 |

### 工具与辅助

| 文件 | 用途 |
|------|------|
| `im2d_common.h` | **im2d 通用工具函数**。提供版本查询（querystring）、错误码转字符串（imStrError）、API 头版本校验（imcheckHeader）、Job 校验（imcheck）、同步等待（imsync）、全局配置（imconfig）等辅助功能。 |
| `im2d_expand.h` | **im2d Android 平台扩展接口（预留）**。预定义了 GraphicBuffer/AHardwareBuffer 的导入和包装接口声明，当前所有函数均为注释状态，启用时需取消注释并链接 Android 系统库。 |

### RGA 旧版兼容层与封装

| 文件 | 用途 |
|------|------|
| `drmrga.h` | **DRM RGA 接口定义**。定义 DRM 缓冲区结构体（bo_t）、RGA 请求结构体（rga_req）、RGA 信息结构体（rga_info）等底层数据结构，以及 drm_rga_init 等 DRM 层初始化接口。 |
| `RgaApi.h` | **RGA 旧版 C 接口兼容层**。提供 librga 早期的 C 语言 API 声明（c_RkRgaInit、c_RkRgaBlit、c_RkRgaGetAllocBuffer 等），用于兼容旧代码。新版推荐直接使用 im2d.h 中的接口。 |
| `RockchipRga.h` | **RockchipRga 主操作类（C++ 单例模式）**。封装 RGA 设备初始化/反初始化、缓冲区分配/释放/映射、Blit 合成、颜色填充、调色板操作等核心功能，同时支持 Android（GraphicBuffer）和 Linux（DRM DMA-BUF）平台。 |
| `RgaUtils.h` | **RGA 工具函数**。提供像素格式查询（bpp、步长）、图像文件读写、格式字符串转换、FBC 压缩格式支持等调试辅助函数。 |
| `RgaSingleton.h` | **线程安全单例模式模板类**。使用互斥锁实现双重检查锁定，确保多线程环境下只创建唯一实例。 |
| `RgaMutex.h` | **RGA 自定义互斥锁**。封装 pthread mutex，提供 lock/unlock/tryLock/timedLock 接口及 Autolock RAII 守卫类，附带线程安全注解宏。 |
| `GrallocOps.h` | **Android Gralloc 操作封装**。仅在 Android 平台编译，提供 RkRgaGetHandleFd、RkRgaGetHandleAttributes、RkRgaGetHandleMapAddress 等 GraphicBuffer 辅助操作函数。 |

---

## 目录结构

```
include/
├── im2d.h                  # im2d C 总入口
├── im2d.hpp                # im2d C++ 扩展入口
├── im2d_type.h             # 类型定义
├── im2d_version.h          # 版本号
├── im2d_buffer.h           # 缓冲区导入/包装
├── im2d_single.h           # 单次操作 API
├── im2d_task.h             # Job/Task 模式 API
├── im2d_mpi.h              # 多进程接口
├── im2d_common.h           # 通用工具函数
├── im2d_expand.h           # Android 扩展（预留）
├── rga.h                   # 像素格式/色彩空间定义
├── drmrga.h                # DRM RGA 接口
├── RgaApi.h                # 旧版 C 接口
├── RockchipRga.h           # C++ 主操作类
├── RgaUtils.h              # 工具函数
├── RgaSingleton.h          # 单例模式模板
├── RgaMutex.h              # 互斥锁
└── GrallocOps.h            # Android Gralloc 封装
```

## 依赖关系

- `im2d.h` 是所有 C 用户的首要入口，内部聚合了 `im2d_type.h`、`im2d_buffer.h`、`im2d_common.h`、`im2d_single.h`、`im2d_task.h`、`im2d_mpi.h`
- `im2d.hpp` 是 C++ 用户的首要入口，在 `im2d.h` 基础上增加 `im2d_expand.h`
- `RockchipRga.h` 依赖 `drmrga.h`、`GrallocOps.h`、`RgaUtils.h`、`rga.h`、`RgaSingleton.h`
- `RgaSingleton.h` 依赖 `RgaMutex.h`
