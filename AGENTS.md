# AGENTS.md - RKNN YOLO Detection Platform

## Build Commands

### RK3588 - Cross-compile (CLI only)
```bash
./build-linux_RK3588.sh
cd install/rknn_yolov5_demo_Linux/
./rknn_yolov5_demo ./model/RK3588/yolov5s-640-640.rknn 0
```

### RK3588 - Native build with Qt
```bash
./build-linux_RK3588_qt.sh
cd install/rknn_yolov5_qt_demo_Linux/
./yolo_rk3588_qt
```

## Platform Detection

CMake auto-detects platform:
- **RK3588**: Linux aarch64 (cross or native)
- **Other Linux**: Generic fallback (not supported)

Use `-DENABLE_QT=OFF` to disable Qt GUI on RK3588.

## Dependencies

| Required | Notes |
|----------|-------|
| aarch64-linux-gnu toolchain, RKNN SDK (in `include/`), OpenCV (system), RGA (in `include/3rdparty/`), Qt5 (optional) | Qt5 not found = CLI mode only |

## Build Outputs

- **RK3588 CLI**: `install/rknn_yolov5_demo_Linux/`
- **RK3588 Qt**: `install/rknn_yolov5_qt_demo_Linux/`

## Important Notes

1. **RK3588 paths**: CMakeLists.txt expects RKNN SDK at `../../runtime/RK3588/` relative to project root. If building in different location, update `RKNN_API_PATH`.

2. **Performance tuning**: Run `performance.sh` as root to lock CPU/NPU frequencies for benchmarking.

3. **Config file**: `config.json` in project root controls model, detection parameters, and thread count.

4. **Model files**: RKNN models go in `model/RK3588/`.

## Architecture

- **Entry points**: `src/main.cpp` (CLI), `src/rk/main_rk.cpp` (Qt GUI)
- **Core logic**: `include/core/` (types, config, logging), `include/engine/` (RKNN engine)
- **UI**: Qt-based in `include/ui/` and `src/rk/`
- **Threading**: `include/rknnPool.hpp` implements thread pool for RKNN inference
