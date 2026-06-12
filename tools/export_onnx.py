"""
export_onnx.py — PyTorch (ultralytics) → ONNX

Usage:
    python tools/export_onnx.py --weights yolov5s.pt --output model/yolov5s.onnx
    python tools/export_onnx.py --weights yolo11n.pt --output model/yolo11n.onnx --opset 12
"""

import argparse
import sys
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description="Export YOLO to ONNX")
    parser.add_argument("--weights", required=True, help="PyTorch weights (.pt)")
    parser.add_argument("--output", default=None, help="Output .onnx path")
    parser.add_argument("--imgsz", type=int, default=640, help="Input size")
    parser.add_argument("--opset", type=int, default=17, help="ONNX opset version")
    parser.add_argument("--simplify", action="store_true", help="Run onnxsim")
    args = parser.parse_args()

    try:
        from ultralytics import YOLO
    except ImportError:
        print("Please install ultralytics: pip install ultralytics")
        sys.exit(1)

    model = YOLO(args.weights)
    output = args.output or Path(args.weights).stem + ".onnx"
    model.export(format="onnx", imgsz=args.imgsz, opset=args.opset, simplify=args.simplify)
    print(f"[OK] Exported to {output}")

if __name__ == "__main__":
    main()
