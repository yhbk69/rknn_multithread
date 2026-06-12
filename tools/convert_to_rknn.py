"""
convert_to_rknn.py — ONNX → RKNN (for RK3588 NPU)

Prerequisites:
    pip install rknn-toolkit2

Usage:
    # YOLOv5s
    python tools/convert_to_rknn.py --onnx model/yolov5s.onnx --output model/yolov5s.rknn --target rk3588

    # YOLO11 (must use Rockchip optimized ONNX from airockchip/ultralytics_yolo11)
    python tools/convert_to_rknn.py --onnx model/yolo11_optimized.onnx --output model/yolo11.rknn --target rk3588
"""

import argparse
import sys


def main():
    parser = argparse.ArgumentParser(description="Convert ONNX to RKNN")
    parser.add_argument("--onnx", required=True, help="Input ONNX model")
    parser.add_argument("--output", required=True, help="Output .rknn file")
    parser.add_argument("--target", default="rk3588", choices=["rk3588", "rk3566", "rk3568"])
    parser.add_argument("--quant", action="store_true", help="Enable INT8 quantization")
    parser.add_argument("--dataset", default=None, help="Dataset for quantization (images dir)")
    args = parser.parse_args()

    try:
        from rknn.api import RKNN
    except ImportError:
        print("ERROR: rknn-toolkit2 not installed.")
        print("Install from: https://github.com/airockchip/rknn-toolkit2")
        sys.exit(1)

    rknn = RKNN(verbose=True)

    # Config
    print(f"[1/4] Loading ONNX: {args.onnx}")
    ret = rknn.load_onnx(model=args.onnx)
    if ret != 0:
        print(f"ERROR: load_onnx failed, ret={ret}")
        sys.exit(1)

    print(f"[2/4] Building RKNN model for {args.target}...")
    ret = rknn.build(
        do_quantization=args.quant,
        dataset=args.dataset if args.quant else None,
        target_platform=args.target,
        optimization_level=3,
    )
    if ret != 0:
        print(f"ERROR: build failed, ret={ret}")
        sys.exit(1)

    print(f"[3/4] Exporting to {args.output}...")
    ret = rknn.export_rknn(args.output)
    if ret != 0:
        print(f"ERROR: export failed, ret={ret}")
        sys.exit(1)

    rknn.release()
    print(f"[4/4] Done: {args.output}")


if __name__ == "__main__":
    main()
