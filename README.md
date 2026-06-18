# RKNN YOLO 多线程检测平台

RK3588 NPU 多线程 YOLO 目标检测系统，支持 YOLOv5s + YOLO11，带 Qt GUI 界面，WebSocket 远程通信。

| 推理后端 | 模型支持 |
|----------|----------|
| RKNN SDK (NPU 多线程) | YOLOv5s / YOLO11 |

---

## 环境要求

| 组件 | 说明 |
|------|------|
| RKNN SDK | 已内置在 `include/` 中 |
| OpenCV | 系统需预装 |
| RGA | 已内置 (硬件加速预处理) |
| Qt 5.x | 可选，无桌面环境可用 MJPEG/WS 推流替代 |

---

## 快速编译

```bash
# 1. 进入项目目录
cd /path/to/rknn_Multithread

# 2. 无 GUI 版本（命令行推理）
./build-linux_RK3588.sh

# 3. 编译完成后运行
cd install/rknn_yolov5_demo_Linux
./rknn_yolov5_demo ./model/RK3588/yolov5s-640-640.rknn 0

# 4. Qt GUI 版本（需要板子上装有 Qt5）
./build-linux_RK3588_qt.sh
cd install/rknn_yolov5_qt_demo_Linux
./yolo_rk3588_qt
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
rknn_Multithread/
├── CMakeLists.txt             # CMake 构建
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
│   ├── rknnPool.hpp           # RKNN 多线程池
│   ├── rkYolov5s.hpp          # YOLOv5s RKNN
│   └── rknn_api.h             # RKNN API
│
├── src/                       # 实现文件
│   ├── main.cpp               # CLI 入口
│   ├── rk/                    # RK3588 Qt 入口
│   ├── postprocess.cpp        # YOLOv5s 后处理
│   ├── preprocess.cpp         # YOLOv5s 预处理
│   └── rkYolov5s.cpp          # YOLOv5s RKNN 推理
│
├── model/                     # 模型文件
│   ├── RK3588/
│   │   └── yolov5s-640-640.rknn
│   └── coco_80_labels_list.txt
│
├── tools/                     # 工具脚本
└── others/                    # 参考项目（不参与编译）
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
