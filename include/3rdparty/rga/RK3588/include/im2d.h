/*
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 * Authors:
 *  PutinLee <putin.lee@rock-chips.com>
 *  Cerf Yu <cerf.yu@rock-chips.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _im2d_h_
#define _im2d_h_

/*
 * im2d 总入口头文件（C 接口），聚合包含所有 im2d 子模块：
 *   im2d_version.h  - API 版本号定义
 *   im2d_type.h     - 核心类型、枚举和结构体定义
 *   im2d_common.h   - 通用工具函数（查询、错误信息、同步、配置）
 *   im2d_buffer.h   - 外部缓冲区导入和包装
 *   im2d_single.h   - 单次操作模式 API（复制、缩放、裁剪、旋转、混合等）
 *   im2d_task.h     - Job/Task 模式 API（批量任务提交）
 *   im2d_mpi.h      - 多进程接口（rockit-ko 上下文模式）
 */

#include "im2d_version.h"
#include "im2d_type.h"

#include "im2d_common.h"
#include "im2d_buffer.h"
#include "im2d_single.h"
#include "im2d_task.h"
#include "im2d_mpi.h"

#endif /* #ifndef _im2d_h_ */
