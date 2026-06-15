# AGENTS.md - YOLO Detection Platform

## Build Commands

### Windows
```powershell
# First-time CMake config (only needed once)
cmake -S . -B build-win -G "Visual Studio 18 2026" -A x64 -DQt5_DIR="C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5"

# Build Release
cmake --build build-win --config Release

# Run
.\build-win\Release\yolo_win.exe
```

**ONNX Runtime**: Must be downloaded manually before first build if `build_temp/onnxruntime/` doesn't exist. See README for download URL.

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
- **Windows**: MSVC compiler
- **RK3588**: Linux aarch64 (cross or native)
- **Other Linux**: Generic fallback (not supported)

Use `-DENABLE_QT=OFF` to disable Qt GUI on RK3588.

## Dependencies

| Platform | Required | Notes |
|----------|----------|-------|
| Windows | Visual Studio 2022+, CMake 3.16+, Qt 5.15 (MSVC 2019 64-bit), OpenCV 4.x, ONNX Runtime 1.26.0 | OpenCV path hardcoded to `C:/opencv412/opencv/build` |
| RK3588 | aarch64-linux-gnu toolchain, RKNN SDK (in `include/`), OpenCV (system), RGA (in `include/3rdparty/`), Qt5 (optional) | Qt5 not found = CLI mode only |

## Build Outputs

- **Windows**: `build-win/Release/yolo_win.exe`
- **RK3588 CLI**: `install/rknn_yolov5_demo_Linux/`
- **RK3588 Qt**: `install/rknn_yolov5_qt_demo_Linux/`

## Important Notes

1. **RK3588 paths**: CMakeLists.txt expects RKNN SDK at `../../runtime/RK3588/` relative to project root. If building in different location, update `RKNN_API_PATH`.

2. **ONNX Runtime DLLs**: Windows build copies `onnxruntime.dll` and `onnxruntime_providers_shared.dll` to output directory automatically.

3. **Performance tuning**: Run `performance.sh` as root to lock CPU/NPU frequencies for benchmarking.

4. **Config file**: `config.json` in project root controls model, detection parameters, and thread count.

5. **Model files**: RKNN models go in `model/RK3588/`. ONNX models are gitignored.

## Architecture

- **Entry points**: `src/win/main_win.cpp` (Windows), `src/main.cpp` (RK3588 CLI), `src/rk/main_rk.cpp` (RK3588 Qt)
- **Core logic**: `include/core/` (types, config, logging), `include/engine/` (ONNX/RKNN engines)
- **UI**: Qt-based in `include/ui/` and `src/win/`/`src/rk/`
- **Threading**: `include/rknnPool.hpp` implements thread pool for RKNN inference