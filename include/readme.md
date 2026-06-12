# include 目录说明

本目录包含 RKNN 多线程 YOLOv5s 推理项目的所有头文件及动态库，按来源分为自研模块、Rockchip 官方 SDK、第三方开源库三类。

---

## 头文件说明

### 自研模块

| 文件 | 用途 |
|------|------|
| `coreNum.hpp` | NPU 核心轮询绑定工具。通过互斥锁 + 取模轮询方式，将多线程推理任务均匀分配到 RK3588 的 3 个 NPU 核心上，实现负载均衡。 |
| `drm_func.h` | DRM（Direct Rendering Manager）内存管理函数声明。封装 Rockchip DRM 驱动接口，提供 DRM 设备初始化、物理连续缓冲区分配/释放、设备反初始化等功能。用于零拷贝场景下分配 NPU 可访问的 DMA 缓冲区。 |
| `postprocess.h` | YOLOv5 目标检测后处理模块。定义检测结果数据结构（`detect_result_t`、`detect_result_group_t`），声明三尺度特征图解码、置信度过滤、NMS 非极大值抑制等核心后处理函数。 |
| `preprocess.h` | YOLOv5 图像预处理模块。提供 Letterbox 等比缩放填充和 RGA 硬件加速缩放两种预处理方式，将输入图像转换为模型所需的标准尺寸。 |
| `rga_func.h` | RGA（Raster Graphics Accelerator）2D 图形加速函数声明。封装 Rockchip RGA 驱动，动态加载 `librga.so` 并获取函数指针，提供快速（DMA 物理地址）和慢速（虚拟地址）两种图像缩放接口。 |
| `rknnPool.hpp` | RKNN 模型线程池管理器。模板类，基于第三方 dpool 线程池实现多模型实例的异步并发推理，通过轮询分配任务到不同模型实例实现负载均衡。支持第一个实例加载权重、后续实例共享权重的内存优化策略。 |
| `rkYolov5s.hpp` | YOLOv5s 目标检测推理类。封装 RKNN YOLOv5s 模型从加载、初始化到推理的完整生命周期，包括 RKNN 上下文管理、张量属性配置、多线程安全推理等。 |

### Rockchip 官方 SDK

| 文件 | 用途 | 来源 |
|------|------|------|
| `rknn_api.h` | RKNN（Rockchip Neural Network）API 主头文件。定义所有 RKNN 核心接口：上下文管理（`rknn_init`/`rknn_destroy`）、推理执行（`rknn_run`/`rknn_outputs_get`）、张量内存管理（零拷贝接口）、NPU 核心掩码设置、性能查询等。同时定义了所有数据结构（`rknn_tensor_attr`、`rknn_input`、`rknn_output` 等）和错误码。 | Rockchip rknn-toolkit2 SDK |
| `rknn_matmul_api.h` | RKNN 矩阵乘法 API。基于 NPU 提供高性能矩阵乘法（C = A × B），支持 int8 和 float16 数据类型，支持普通布局和性能优化布局（适用于 RK356x/RK3588 平台），包含 NPU 核心掩码设置和阻塞模式执行接口。 | Rockchip rknn-toolkit2 SDK |

### 第三方开源库

| 文件 | 用途 | 来源 |
|------|------|------|
| `ThreadPool.hpp` | 动态线程池库 dpool。按需创建工作线程，最大线程数可配置；空闲线程超时（2秒）自动回收以减少资源占用；线程安全，支持 `submit` 提交任务并返回 `std::future` 获取异步结果。 | https://github.com/senlinzhan/dpool |

---

## 动态库说明

| 文件 | 用途 |
|------|------|
| `librknnrt.so` | RKNN 运行时动态库。NPU 推理的核心运行时库，包含模型解析、图编译、NPU 驱动交互、算子执行等底层实现。`rknn_api.h` 中声明的所有函数均由本库提供。 |
| `librknn_api.so` | RKNN API 包装动态库。通常为 `librknnrt.so` 的符号链接或兼容性包装层，提供稳定的 API ABI 接口供上层应用链接。实际功能委托给 `librknnrt.so`。 |

---

## 子目录说明

| 目录 | 说明 |
|------|------|
| `3rdparty/` | 第三方依赖库头文件，当前包含 RGA（Raster Graphics Accelerator）的 C/C++ API 头文件（Rockchip 官方提供），存放在 `3rdparty/rga/RK3588/` 下。 |
