#!/bin/bash
set -e

# RK3588 native build script (with Qt GUI)
# Run directly on RK3588 device

ROOT_PWD=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR=${ROOT_PWD}/build/build_linux_aarch64_qt

mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

cmake ../.. \
  -DENABLE_QT=ON \
  -DCMAKE_INSTALL_PREFIX=${ROOT_PWD}/install

make -j$(nproc)
make install

echo "Build complete. Run:"
echo "  cd install/rknn_yolov5_qt_demo_Linux/"
echo "  ./yolo_rk3588_qt"
