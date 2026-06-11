// coreNum.hpp - RKNN 多线程推理核心绑定工具
// 提供 NPU 核心轮询分配功能，用于多线程场景下将模型绑定到不同的 NPU 核心
#ifndef CORENUM_H
#define CORENUM_H

#include <stdio.h>

#include "rknn_api.h"

// RK3588 NPU 核心数量，用于轮询分配
const int RK3588 = 3;

// 设置模型需要绑定的核心（线程安全，轮询分配）
int get_core_num()
{
    static int core_num = 0;
    static std::mutex mtx;

    std::lock_guard<std::mutex> lock(mtx);

    int temp = core_num % RK3588;
    core_num++;
    return temp;
}
#endif