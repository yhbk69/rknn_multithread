# 统一 YOLO 检测平台

跨平台 YOLO 目标检测系统，支持 YOLOv5s + YOLO11，带 Qt GUI 界面，WebSocket 远程通信。

| 平台 | 推理后端 | 模型支持 |
|------|----------|----------|
| Windows | ONNX Runtime (CPU) | YOLO11 |
| RK3588 | RKNN SDK (NPU 多线程) | YOLOv5s / YOLO11 |

---

## 环境要求

### Windows

| 组件 | 版本 | 路径示例 |
|------|------|----------|
| Visual Studio | 2022/2025 (MSVC) | `C:\Program Files\Microsoft Visual Studio\18\Community\` |
| CMake | >= 3.16 | `C:\Program Files\CMake\bin\cmake.exe` |
| Qt 5.15 | MSVC 2019 64-bit | `C:\Qt\5.15.2\msvc2019_64\` |
| OpenCV | 4.x | `C:\opencv412\opencv\build\` |
| ONNX Runtime | 1.26.0 | 自动下载到 `build_temp/` |

### RK3588

| 组件 | 说明 |
|------|------|
| RKNN SDK | 已内置在 `include/` 中 |
| OpenCV | 系统需预装 |
| RGA | 已内置 (硬件加速预处理) |
| Qt 5.x | 可选，无桌面环境可用 MJPEG/WS 推流替代 |

---

## 快速编译

### Windows

```powershell
# 1. 进入项目目录
cd D:\tmp\rknn_multithread

# 2. 首次配置 CMake（只需一次）
cmake -S . -B build-win -G "Visual Studio 18 2026" -A x64 `
  -DQt5_DIR="C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5"

# 3. 编译 Release
cmake --build build-win --config Release

# 4. 运行
.\build-win\Release\yolo_win.exe
```

> **提示**：也可用 Visual Studio 打开 `build-win\yolo_detection_platform.sln` 在 IDE 中编译和调试。

> **首次编译**：CMake 会自动检测 OpenCV (`C:\opencv412\opencv\build`) 和 ONNX Runtime (`build_temp\onnxruntime\`)。如果 ONNX Runtime 不存在，需要先下载：
> ```powershell
> mkdir build_temp
> Invoke-WebRequest -Uri "https://github.com/microsoft/onnxruntime/releases/download/v1.26.0/onnxruntime-win-x64-1.26.0.zip" `
>   -OutFile "build_temp\onnxruntime.zip"
> Expand-Archive build_temp\onnxruntime.zip -DestinationPath build_temp\onnxruntime
> ```

### RK3588

```bash
# 1. 进入项目目录
cd /path/to/rknn_multithread

# 2. 无 GUI 版本（命令行推理）
./build-linux_RK3588.sh

# 3. 编译完成后运行
cd install/rknn_yolov5_demo_Linux
./rknn_yolov5_demo ./model/RK3588/yolov5s-640-640.rknn 0

# 4. Qt GUI 版本（需要板子上装有 Qt5）
./build-linux_RK3588_qt.sh
cd install/rknn_yolov5_qt_demo_Linux
./rknn_yolov5_qt_demo
```

---

## 配置文件

项目根目录的 `config.json`：

```json
{
  "model": {
    "path": "./model/RK3588/yolov5s-640-640.rknn",
    "label_file": "coco_80_labels_list.txt",
    "input_width": 640,
    "input_height": 640
  },
  "detection": {
    "nms_threshold": 0.45,
    "box_threshold": 0.25,
    "class_num": 80
  },
  "runtime": {
    "thread_num": 3
  }
}
```

---

## 项目结构

```
rknn_multithread/
├── CMakeLists.txt             # 双平台 CMake
├── config.json                # 运行时配置
├── README.md
├── PROJECT_PLAN.md            # 开发计划与进度
│
├── include/                   # 头文件
│   ├── core/                  # 核心类型、配置、日志
│   ├── engine/                # 推理引擎接口与实现
│   ├── server/                # WebSocket / MJPEG / HTTP
│   ├── pipeline/              # 预处理 / 后处理
│   ├── manager/               # 推理管理 / 摄像头管理 / 模型管理
│   ├── io/                    # 视频源 / 视频录制
│   ├── ui/                    # Qt GUI 组件
│   │
│   ├── rknnPool.hpp           # RKNN 多线程池（已有）
│   ├── rkYolov5s.hpp          # YOLOv5s RKNN（已有）
│   └── rknn_api.h             # RKNN API（已有）
│
├── src/                       # 实现文件
│   ├── main_win.cpp           # Windows 入口
│   ├── onnx_engine.cpp        # ONNX 引擎实现
│   ├── postprocess.cpp        # YOLOv5s 后处理（已有）
│   ├── preprocess.cpp         # YOLOv5s 预处理（已有）
│   └── rkYolov5s.cpp          # YOLOv5s RKNN 推理（已有）
│
├── model/                     # 模型文件
│   ├── RK3588/
│   │   └── yolov5s-640-640.rknn
│   └── coco_80_labels_list.txt
│
├── tools/                     # 工具脚本
├── build-win/                 # Windows 构建输出
└── others/                    # 参考项目（不参与编译）
    ├── yolo11_2/              # Qt GUI 参考
    └── rknn/rknn_model_zoo/   # RKNN 模型参考
```

---

## 参考项目

- [rknpu2](https://github.com/rockchip-linux/rknpu2) — RKNN 原始示例
- [rknn-multi-threaded](https://github.com/leafqycc/rknn-multi-threaded) — 多线程 Python 实现
- [rknn_model_zoo](https://github.com/airockchip/rknn_model_zoo) — YOLO11 RKNN 优化模型
- [dpool](https://github.com/senlinzhan/dpool) — C++ 线程池

## Acknowledgments

- https://github.com/rockchip-linux/rknpu2
- https://github.com/senlinzhan/dpool
- https://github.com/ultralytics/yolov5
- https://github.com/airockchip/rknn_model_zoo
