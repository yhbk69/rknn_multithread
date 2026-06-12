#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
YOLO ONNX → RKNN 转换工具（增强版）

支持以下模型格式：
  1. 标准 YOLO11（ultralytics 原生 export）  — 单输出 [1, C+4, 8400]
  2. Rockchip 优化 YOLO11（airockchip fork） — 9 输出（3尺度×3分支 + score_sum）
  3. YOLOv5s（ultralytics / rknpu2）         — 3 输出（3尺度特征图）

用法：
  python convert.py <onnx_model> <platform> [选项]

示例：
  # 标准 YOLO11 (ultralytics export)
  python convert.py yolo11n.onnx rk3588

  # Rockchip 优化 YOLO11
  python convert.py yolo11n_rkopt.onnx rk3588 --dataset dataset.txt

  # YOLOv5s
  python convert.py yolov5s.onnx rk3588 --dtype i8

  # 不量化 (FP16)
  python convert.py yolo11n.onnx rk3588 --dtype fp
"""

import sys
import os
import json
import argparse
import numpy as np

# ─────────────────────────────────────────────
# 模型类型自动检测
# ─────────────────────────────────────────────

MODEL_TYPE_STANDARD_YOLO11 = "standard_yolo11"
MODEL_TYPE_RKOPT_YOLO11    = "rkopt_yolo11"
MODEL_TYPE_YOLOV5          = "yolov5"
MODEL_TYPE_UNKNOWN         = "unknown"


def inspect_onnx_model(model_path):
    """
    读取 ONNX 模型的输入/输出信息，自动判断模型类型。

    Returns:
        dict: {
            'model_type': str,
            'input_shape': list,       # 如 [1, 3, 640, 640]
            'num_outputs': int,
            'output_shapes': list,     # 每个输出的 shape
            'num_classes': int or None,
            'description': str
        }
    """
    try:
        import onnx
    except ImportError:
        print("[WARN] onnx 库未安装，无法自动检测模型类型。")
        print("       请安装: pip install onnx")
        return None

    model = onnx.load(model_path)
    graph = model.graph

    # 提取输入信息
    inputs = []
    for inp in graph.input:
        shape = []
        for dim in inp.type.tensor_type.shape.dim:
            if dim.dim_value > 0:
                shape.append(dim.dim_value)
            else:
                shape.append(-1)  # 动态维度
        inputs.append({'name': inp.name, 'shape': shape})

    # 提取输出信息
    outputs = []
    for out in graph.output:
        shape = []
        for dim in out.type.tensor_type.shape.dim:
            if dim.dim_value > 0:
                shape.append(dim.dim_value)
            else:
                shape.append(-1)
        outputs.append({'name': out.name, 'shape': shape})

    num_outputs = len(outputs)

    # ── 判断逻辑 ──

    # 情况 1: 单输出，形状类似 [1, C, N]  → 标准 YOLO11
    if num_outputs == 1:
        shape = outputs[0]['shape']
        if len(shape) == 3 and shape[0] == 1:
            channels = shape[1]   # num_classes + 4
            anchors = shape[2]    # 8400
            num_classes = channels - 4
            return {
                'model_type': MODEL_TYPE_STANDARD_YOLO11,
                'input_shape': inputs[0]['shape'] if inputs else None,
                'num_outputs': num_outputs,
                'output_shapes': [o['shape'] for o in outputs],
                'num_classes': num_classes,
                'num_anchors': anchors,
                'description': (
                    f"标准 YOLO11 (ultralytics export)\n"
                    f"    输出: [{shape[0]}, {channels}, {anchors}]\n"
                    f"    类别数: {num_classes}, 锚点数: {anchors}\n"
                    f"    后处理: 转置 → 逐锚点 argmax → 置信度过滤 → NMS"
                )
            }

    # 情况 2: 9 个输出 → Rockchip 优化 YOLO11
    if num_outputs == 9:
        # 分析输出形状，推断类别数
        # 9 输出 = 3 尺度 × 3 分支 (box_dfl[64], class_scores[C], score_sum[1])
        class_channels = None
        for out in outputs:
            s = out['shape']
            if len(s) == 4 and s[1] not in [64, 1]:
                class_channels = s[1]
                break

        num_classes = class_channels if class_channels else None
        return {
            'model_type': MODEL_TYPE_RKOPT_YOLO11,
            'input_shape': inputs[0]['shape'] if inputs else None,
            'num_outputs': num_outputs,
            'output_shapes': [o['shape'] for o in outputs],
            'num_classes': num_classes,
            'description': (
                f"Rockchip 优化 YOLO11 (airockchip/ultralytics_yolo11)\n"
                f"    输出: {num_outputs} 个张量 (3尺度×3分支)\n"
                f"    类别数: {num_classes}\n"
                f"    后处理: DFL解码 → 逐尺度解码 → 合并 → 过滤 → NMS"
            )
        }

    # 情况 3: 3 个输出 → 可能是 YOLOv5
    if num_outputs == 3:
        return {
            'model_type': MODEL_TYPE_YOLOV5,
            'input_shape': inputs[0]['shape'] if inputs else None,
            'num_outputs': num_outputs,
            'output_shapes': [o['shape'] for o in outputs],
            'num_classes': None,  # YOLOv5 的类别数嵌在输出维度中，需要额外计算
            'description': (
                f"YOLOv5 (3尺度特征图输出)\n"
                f"    输出: {num_outputs} 个张量\n"
                f"    后处理: 逐尺度 anchor 解码 → 置信度过滤 → NMS"
            )
        }

    # 无法识别
    return {
        'model_type': MODEL_TYPE_UNKNOWN,
        'input_shape': inputs[0]['shape'] if inputs else None,
        'num_outputs': num_outputs,
        'output_shapes': [o['shape'] for o in outputs],
        'num_classes': None,
        'description': f"未知模型格式 ({num_outputs} 个输出)"
    }


# ─────────────────────────────────────────────
# 校准数据集准备
# ─────────────────────────────────────────────

def prepare_calibration_dataset(dataset_path, input_shape, num_images=20):
    """
    准备量化校准数据。

    Args:
        dataset_path: 图片路径列表文件（每行一个路径），或 None
        input_shape: 模型输入形状 [1, C, H, W]
        num_images: 需要的校准图片数量

    Returns:
        str: 实际的 dataset 文件路径
    """
    if dataset_path and os.path.isfile(dataset_path):
        # 验证文件中列出的图片是否存在
        with open(dataset_path, 'r') as f:
            lines = [l.strip() for l in f.readlines() if l.strip()]
        valid = [l for l in lines if os.path.isfile(l)]
        if len(valid) < 1:
            print(f"[WARN] 校准数据文件中的图片路径均无效，将生成随机校准数据。")
        else:
            print(f"[INFO] 使用校准数据: {dataset_path} ({len(valid)} 张有效图片)")
            return dataset_path

    # 没有提供 dataset，生成随机校准数据
    print(f"[INFO] 未提供校准数据，生成 {num_images} 张随机图片用于量化校准...")
    calib_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '_calib_temp')
    os.makedirs(calib_dir, exist_ok=True)

    h, w = input_shape[2], input_shape[3]
    dataset_file = os.path.join(calib_dir, 'dataset.txt')

    try:
        import cv2
        for i in range(num_images):
            img = np.random.randint(0, 256, (h, w, 3), dtype=np.uint8)
            path = os.path.join(calib_dir, f'calib_{i:04d}.jpg')
            cv2.imwrite(path, img)
        with open(dataset_file, 'w') as f:
            for i in range(num_images):
                f.write(os.path.join(calib_dir, f'calib_{i:04d}.jpg') + '\n')
        print(f"[INFO] 随机校准图片已生成: {calib_dir}")
    except ImportError:
        # 没有 OpenCV，用纯 numpy 写 BMP（简单格式）
        print("[WARN] OpenCV 未安装，使用 numpy 生成 .npy 校准数据")
        calib_data = np.random.randint(0, 256, (num_images, h, w, 3), dtype=np.uint8)
        npy_path = os.path.join(calib_dir, 'calib_data.npy')
        np.save(npy_path, calib_data)
        with open(dataset_file, 'w') as f:
            f.write(npy_path + '\n')

    return dataset_file


# ─────────────────────────────────────────────
# RKNN 转换
# ─────────────────────────────────────────────

def convert_to_rknn(onnx_path, output_path, platform, do_quant,
                    dataset_path, model_info, mean_values, std_values):
    """
    执行 ONNX → RKNN 转换。

    Args:
        onnx_path: ONNX 模型路径
        output_path: RKNN 输出路径
        platform: 目标平台 (如 'rk3588')
        do_quant: 是否量化 (True=i8/u8, False=fp)
        dataset_path: 校准数据路径
        model_info: 模型检测结果 dict
        mean_values: 归一化均值
        std_values: 归一化标准差
    """
    from rknn.api import RKNN

    print('=' * 60)
    print(f'ONNX → RKNN 转换')
    print(f'  模型: {onnx_path}')
    print(f'  平台: {platform}')
    print(f'  量化: {"i8" if do_quant else "fp (无量化)"}')
    print('=' * 60)

    # 打印模型检测结果
    if model_info:
        print(f'\n[模型检测] {model_info["description"]}\n')

    # 创建 RKNN 对象
    rknn = RKNN(verbose=True)

    # 配置预处理参数
    # mean=[0,0,0], std=[255,255,255] 等价于 input/255.0 归一化
    # 这对标准 ultralytics 导出的 YOLO 模型是正确的
    print('--> 配置模型参数')
    rknn.config(
        mean_values=mean_values,
        std_values=std_values,
        target_platform=platform,
    )
    print('done')

    # 加载 ONNX 模型
    print('--> 加载 ONNX 模型')
    ret = rknn.load_onnx(model=onnx_path)
    if ret != 0:
        print(f'[ERROR] ONNX 模型加载失败! ret={ret}')
        print('  可能原因:')
        print('  - ONNX 文件损坏或格式不兼容')
        print('  - 某些算子不被 RKNN 支持（检查 verbose 输出）')
        print('  - 尝试使用 opset_version=12 重新导出 ONNX')
        rknn.release()
        return False
    print('done')

    # 构建 RKNN 模型
    print('--> 构建 RKNN 模型')
    if do_quant:
        if not dataset_path or not os.path.isfile(dataset_path):
            print('[ERROR] 量化模式需要提供校准数据集!')
            print('  用法: python convert.py model.onnx rk3588 --dataset dataset.txt')
            print('  dataset.txt 每行一个图片路径')
            rknn.release()
            return False
        ret = rknn.build(do_quantization=True, dataset=dataset_path)
    else:
        ret = rknn.build(do_quantization=False)

    if ret != 0:
        print(f'[ERROR] RKNN 模型构建失败! ret={ret}')
        print('  可能原因:')
        print('  - 校准数据格式不正确')
        print('  - 模型包含不支持的算子')
        print('  - 尝试 --dtype fp 跳过量化')
        rknn.release()
        return False
    print('done')

    # 导出 RKNN 模型
    print('--> 导出 RKNN 模型')
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    ret = rknn.export_rknn(output_path)
    if ret != 0:
        print(f'[ERROR] RKNN 模型导出失败! ret={ret}')
        rknn.release()
        return False
    print(f'done → {output_path}')

    # 释放
    rknn.release()
    return True


# ─────────────────────────────────────────────
# 元数据保存
# ─────────────────────────────────────────────

def save_model_metadata(output_path, model_info, platform, do_quant):
    """
    保存模型元数据 JSON 文件，供 C++ 端加载时参考。
    文件名为 <rknn_model>.json，与 RKNN 模型同目录。
    """
    if not model_info:
        return

    metadata = {
        'model_type': model_info['model_type'],
        'platform': platform,
        'quantized': do_quant,
        'input_shape': model_info['input_shape'],
        'num_outputs': model_info['num_outputs'],
        'output_shapes': model_info['output_shapes'],
        'num_classes': model_info.get('num_classes'),
        'description': model_info['description'],
        'source_onnx': os.path.basename(output_path).replace('.rknn', '.onnx'),
    }

    meta_path = output_path.replace('.rknn', '.json')
    with open(meta_path, 'w', encoding='utf-8') as f:
        json.dump(metadata, f, ensure_ascii=False, indent=2)
    print(f'[INFO] 模型元数据已保存: {meta_path}')


# ─────────────────────────────────────────────
# 清理临时文件
# ─────────────────────────────────────────────

def cleanup_temp():
    """清理自动生成的临时校准数据"""
    calib_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '_calib_temp')
    if os.path.exists(calib_dir):
        import shutil
        shutil.rmtree(calib_dir, ignore_errors=True)


# ─────────────────────────────────────────────
# 主入口
# ─────────────────────────────────────────────

def parse_args():
    parser = argparse.ArgumentParser(
        description='YOLO ONNX → RKNN 转换工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 标准 YOLO11 (ultralytics export)
  python convert.py yolo11n.onnx rk3588

  # 指定校准数据集
  python convert.py yolo11n.onnx rk3588 --dataset dataset.txt

  # 不量化 (FP16, 精度最高)
  python convert.py yolo11n.onnx rk3588 --dtype fp

  # 自定义输出路径
  python convert.py yolo11n.onnx rk3588 --output my_model.rknn

  # 自定义归一化参数 (一般不需要改)
  python convert.py yolo11n.onnx rk3588 --mean 0 0 0 --std 255 255 255

支持的平台: rk3562, rk3566, rk3568, rk3588, rk3576, rk1808, rv1109, rv1126
支持的量化类型: i8 (默认), fp (无量化)
        """
    )

    parser.add_argument('onnx_model', type=str,
                        help='ONNX 模型文件路径')
    parser.add_argument('platform', type=str,
                        choices=['rk3562', 'rk3566', 'rk3568', 'rk3588',
                                 'rk3576', 'rk1808', 'rv1109', 'rv1126'],
                        help='目标 NPU 平台')
    parser.add_argument('--dtype', type=str, default='i8',
                        choices=['i8', 'u8', 'fp'],
                        help='量化类型 (默认: i8). i8/u8=量化, fp=不量化')
    parser.add_argument('--output', type=str, default=None,
                        help='RKNN 输出路径 (默认: 同目录下 .rknn)')
    parser.add_argument('--dataset', type=str, default=None,
                        help='量化校准数据集文件 (每行一个图片路径). '
                             '不指定时自动生成随机校准数据')
    parser.add_argument('--mean', type=float, nargs=3, default=[0, 0, 0],
                        help='归一化均值 (默认: 0 0 0)')
    parser.add_argument('--std', type=float, nargs=3, default=[255, 255, 255],
                        help='归一化标准差 (默认: 255 255 255, 等价于 /255.0)')
    parser.add_argument('--no-metadata', action='store_true',
                        help='不生成模型元数据 JSON 文件')

    return parser.parse_args()


def main():
    args = parse_args()

    # 验证 ONNX 文件存在
    if not os.path.isfile(args.onnx_model):
        print(f'[ERROR] ONNX 模型文件不存在: {args.onnx_model}')
        sys.exit(1)

    # 确定输出路径
    if args.output:
        output_path = args.output
    else:
        base = os.path.splitext(args.onnx_model)[0]
        output_path = base + '.rknn'

    # 是否量化
    do_quant = args.dtype in ('i8', 'u8')

    # 自动检测模型类型
    print('\n[1/4] 检测模型类型...')
    model_info = inspect_onnx_model(args.onnx_model)
    if model_info:
        print(f'  类型: {model_info["model_type"]}')
        print(f'  {model_info["description"]}')

        if model_info['model_type'] == MODEL_TYPE_UNKNOWN:
            print('\n[WARN] 无法识别模型格式，将尝试直接转换。')
            print('  如果转换失败，请检查 ONNX 模型是否为 YOLO 格式。')
            print('  支持的格式:')
            print('    - 标准 YOLO11: 单输出 [1, C+4, 8400]')
            print('    - RK优化 YOLO11: 9 输出 (3尺度×3分支)')
            print('    - YOLOv5: 3 输出 (3尺度特征图)')
    else:
        print('  跳过检测 (onnx 库未安装)')

    # 准备校准数据
    print('\n[2/4] 准备校准数据...')
    dataset_path = None
    if do_quant:
        if args.dataset:
            dataset_path = args.dataset
            if not os.path.isfile(dataset_path):
                print(f'[ERROR] 校准数据文件不存在: {dataset_path}')
                sys.exit(1)
            print(f'  使用: {dataset_path}')
        elif model_info and model_info.get('input_shape'):
            dataset_path = prepare_calibration_dataset(
                None, model_info['input_shape'])
        else:
            print('[ERROR] 无法确定输入尺寸，请通过 --dataset 指定校准数据')
            sys.exit(1)
    else:
        print('  跳过 (FP 模式无需校准数据)')

    # 执行转换
    print('\n[3/4] 执行转换...')
    mean_values = [args.mean]
    std_values = [args.std]
    success = convert_to_rknn(
        args.onnx_model, output_path, args.platform,
        do_quant, dataset_path, model_info,
        mean_values, std_values
    )

    # 清理临时文件
    cleanup_temp()

    if not success:
        sys.exit(1)

    # 保存元数据
    print('\n[4/4] 保存元数据...')
    if not args.no_metadata and model_info:
        save_model_metadata(output_path, model_info, args.platform, do_quant)

    # 完成
    print('\n' + '=' * 60)
    print('转换完成!')
    print(f'  RKNN 模型: {output_path}')
    if model_info:
        print(f'  模型类型: {model_info["model_type"]}')
        if model_info.get('num_classes'):
            print(f'  类别数:   {model_info["num_classes"]}')
    print()
    print('后续步骤:')
    print(f'  1. 将 {output_path} 部署到 RK3588 设备')
    print(f'  2. 在 config.json 中配置:')
    if model_info:
        print(f'     "model_type": "{model_info["model_type"]}"')
        print(f'     "model_path": "{os.path.basename(output_path)}"')
        if model_info.get('num_classes'):
            print(f'     "num_classes": {model_info["num_classes"]}')
    print('=' * 60)


if __name__ == '__main__':
    main()
