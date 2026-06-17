# 单元测试

## 测试覆盖

### Phase 1 — Pipeline 组件测试（T1-T3）

| 模块 | 文件 | 测试内容 | 依赖 |
|------|------|---------|------|
| `StatsCollector` | `unit/test_stats_collector.cpp` | 计时精度（10ms sleep 误差 < 2ms）、FPS 计算（30 帧间隔）、单调时钟、边界条件（frame_count=0）、avg_fps 计算 | 无 |
| `FrameReader` | `unit/test_frame_reader.cpp` | 无效路径 open 失败、未打开时 read 返回 false、close 不崩溃、readWithRetry 失败、double close 安全 | OpenCV |
| `ResultRenderer` | `unit/test_result_renderer.cpp` | 640x480 QImage 输出格式验证、空 Mat 处理、1x1 最小尺寸、drawFps 不崩溃、连续调用一致性 | OpenCV + Qt5::Gui |

### Phase 2 — 核心逻辑测试（T4-T8, T10-T11）

| # | 模块 | 文件 | 测试内容 | 新增 |
|---|------|------|---------|------|
| T4 | postprocess | `unit/test_postprocess.cpp` | 6 用例：init/deinit、double init、nonexistent path、zero input、single detection | ✅ 2026-06-16 |
| T5 | preprocess | `unit/test_preprocess.cpp` | 6 用例：exact match、wider/taller image、generic aspect ratio、pixel type、non-square target | ✅ 2026-06-17 |
| T6 | config_loader | `unit/test_config_loader.cpp` | 7 用例：full config、missing file defaults、minimal config、corrupted JSON、cascade format、fallback、websocket names | ✅ 2026-06-17 |
| T7 | rknnPool | `unit/test_rknn_pool.cpp` | 6 用例：construction、put/get order、empty get、overflow discard、multi-threaded、thresholds | ✅ 2026-06-17 |
| T8 | coreNum | `unit/test_core_num.cpp` | 6 用例：cycle 0/1/2、channel assignment、instance offset、thread safety、even distribution、constant | ✅ 2026-06-17 |
| T10 | IEngine | `unit/test_iengine.cpp` | 6 用例：interface contract、empty frame、null output、thresholds、multiple init、dimensions | ✅ 2026-06-17 |
| T11 | CascadePipeline | `unit/test_cascade.cpp` | 7 用例：empty init、isCascade、empty result、thresholds safety、put error、get error、ModelConfig defaults/custom | ✅ 2026-06-17 |

### T9 — WebSocket（需要 Qt5，未运行）

| 模块 | 文件 | 测试内容 |
|------|------|---------|
| WebSocket | `unit/test_websocket.cpp` | rate limiting、broadcast empty、start/stop cycle、isAlarmEnabled、clientCount |

## 构建和运行

```bash
# 从项目根目录构建（含全部测试）
cmake -S . -B build -DENABLE_TESTS=ON
cmake --build build

# 运行所有测试
ctest --test-dir build -V

# 运行单个测试
ctest --test-dir build -R test_stats_collector -V
./build/tests/test_stats_collector

# 禁用 Qt 构建（若当前环境无 Qt5）
cmake -S . -B build -DENABLE_TESTS=ON -DENABLE_QT=OFF
```

## 注意事项

- `test_result_renderer` 和 `test_websocket` 需要 Qt5 环境（`ENABLE_QT=ON`）
- `test_preprocess`、`test_cascade` 需要 librga.so（RK3588 RGA 驱动，运行时自动回退 CPU fallback）
- `test_rknn_pool` 使用 MockModel 模拟推理引擎，无需实际 RKNN 硬件
- `test_frame_reader` 会对无效视频路径打印 OpenCV GStreamer 警告，属于正常行为
- `test_core_num` 和 `test_config_loader` 无外部依赖

## 测试目录结构

```
tests/
├── CMakeLists.txt               # 测试构建入口
├── README.md
├── catch_amalgamated.hpp        # Catch2 v3.7.1 头文件
├── catch_amalgamated.cpp        # Catch2 v3.7.1 实现（含 main）
├── fixtures/model/
│   └── test.rknn                # 测试用模型文件（postprocess / cascade）
├── unit/
│   ├── test_stats_collector.cpp
│   ├── test_frame_reader.cpp
│   ├── test_result_renderer.cpp  # 需要 Qt5::Gui
│   ├── test_postprocess.cpp
│   ├── test_preprocess.cpp
│   ├── test_config_loader.cpp
│   ├── test_rknn_pool.cpp
│   ├── test_core_num.cpp
│   ├── test_iengine.cpp
│   ├── test_cascade.cpp
│   └── test_websocket.cpp       # 需要 Qt5，待加入构建
```

## 添加新测试

1. 在 `unit/` 下创建 `test_xxx.cpp`
2. 在 `tests/CMakeLists.txt` 中添加：
   ```cmake
   add_unit_test(test_xxx unit/test_xxx.cpp)
   target_link_libraries(test_xxx ${OpenCV_LIBS})
   ```
3. 构建并运行

## 框架

使用 Catch2 v3.7.1（amalgamated 分发版）：
- 单头文件 + 单实现文件，无需安装
- Header-only 编译模型，通过 `catch_amalgamated.cpp` 提供 `main()` 和 Catch2 实现
- 测试文件只需 `#include "catch_amalgamated.hpp"` 即可使用 `TEST_CASE` / `REQUIRE` 宏
