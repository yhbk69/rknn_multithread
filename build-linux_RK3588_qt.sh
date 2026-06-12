#!/bin/bash
set -e

# RK3588 cross-compile script (with Qt GUI)
# Prerequisites: Qt5 for aarch64, OpenCV, RKNN SDK

GCC_COMPILER=aarch64-linux-gnu
export CC=${GCC_COMPILER}-gcc
export CXX=${GCC_COMPILER}-g++

ROOT_PWD=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR=${ROOT_PWD}/build/build_linux_aarch64_qt

mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

cmake ../.. \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DENABLE_QT=ON

make -j$(nproc)
make install

echo "Build complete. Run:"
echo "  cd install/rknn_yolov5_qt_demo_Linux/"
echo "  ./rknn_yolov5_qt_demo"
