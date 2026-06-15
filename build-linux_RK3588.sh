#!/bin/bash
set -e

# RK3588 cross-compile script
# Prerequisites: aarch64-linux-gnu toolchain, RKNN SDK, OpenCV for aarch64

GCC_COMPILER=aarch64-linux-gnu
export CC=${GCC_COMPILER}-gcc
export CXX=${GCC_COMPILER}-g++

ROOT_PWD=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR=${ROOT_PWD}/build/build_linux_aarch64

mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

cmake ../.. \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DENABLE_QT=OFF \
  -DCMAKE_INSTALL_PREFIX=${ROOT_PWD}/install

make -j$(nproc)
make install

echo "Build complete. Run:"
echo "  cd install/rknn_yolov5_demo_Linux/"
echo "  ./rknn_yolov5_demo ./model/RK3588/yolov5s-640-640.rknn 0"
