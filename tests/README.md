# 单元测试

## 测试了什么

当前覆盖 3 个新拆分出的 pipeline 组件（P1）：

| 模块 | 文件 | 测试内容 |
|------|------|---------|
| `StatsCollector` | `unit/test_stats_collector.cpp` | 计时精度（10ms sleep 误差 < 2ms）、FPS 计算（30 帧间隔）、单调时钟、边界条件（frame_count=0）、avg_fps 计算 |
| `FrameReader` | `unit/test_frame_reader.cpp` | 无效路径 open 失败、未打开时 read 返回 false、close 不崩溃、readWithRetry 失败、double close 安全 |
| `ResultRenderer` | `unit/test_result_renderer.cpp` | 640x480 QImage 输出格式验证、空 Mat 处理、1x1 最小尺寸、drawFps 各种数值不崩溃、连续调用一致性 |

说明：
- `test_result_renderer` 需要 Qt5 环境，当前未编译（`ENABLE_QT=OFF`）
- `test_frame_reader` 会对无效视频路径打印 OpenCV GStreamer 警告，属于正常行为

## 构建和运行

```bash
# 从项目根目录构建
cmake -S . -B build -DENABLE_TESTS=ON
cmake --build build

# 运行所有测试
ctest --test-dir build -V

# 运行单个测试
ctest --test-dir build -R test_stats_collector -V
./build/tests/test_stats_collector
./build/tests/test_frame_reader

# 禁用 Qt 构建（若当前环境无 Qt5）
cmake -S . -B build -DENABLE_TESTS=ON -DENABLE_QT=OFF
```

## 测试目录结构

```
tests/
├── CMakeLists.txt               # 测试构建入口
├── catch_amalgamated.hpp        # Catch2 v3.7.1 头文件（声明 + 宏）
├── catch_amalgamated.cpp        # Catch2 v3.7.1 实现（含 main）
├── README.md
├── unit/                        # 单元测试
│   ├── test_stats_collector.cpp
│   ├── test_frame_reader.cpp
│   └── test_result_renderer.cpp  # 需要 Qt5::Gui
├── fixtures/                    # 测试用数据文件
└── mock/                        # Mock 实现
```

## 添加新测试

1. 在 `unit/` 下创建 `test_xxx.cpp`
2. 在 `tests/CMakeLists.txt` 中添加：
   ```cmake
   add_unit_test(test_xxx unit/test_xxx.cpp ${OpenCV_LIBS})
   ```
3. 构建并运行

## 框架

使用 Catch2 v3.7.1（amalgamated 分发版）：
- 单头文件 + 单实现文件，无需安装
- Header-only 编译模型，通过 `catch_amalgamated.cpp` 提供 `main()` 和 Catch2 实现
- 测试文件只需 `#include "catch_amalgamated.hpp"` 即可使用 `TEST_CASE` / `REQUIRE` 宏
