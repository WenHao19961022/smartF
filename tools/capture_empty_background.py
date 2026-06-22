#!/usr/bin/env python3
"""Capture a robust median reference of the fixed empty refrigerator scene."""

import argparse
from pathlib import Path

import cv2
import numpy as np


def main():
    parser = argparse.ArgumentParser(description="Capture empty-fridge calibration background")
    parser.add_argument("--camera", type=int, default=0)
    parser.add_argument("--output", default="config/fridge_empty_bg.jpg")
    parser.add_argument("--warmup", type=int, default=60)
    parser.add_argument("--frames", type=int, default=60)
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=480)
    args = parser.parse_args()

    capture = cv2.VideoCapture(args.camera, cv2.CAP_V4L2)
    capture.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
    capture.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
    if not capture.isOpened():
        raise RuntimeError(f"Cannot open camera {args.camera}")

    try:
        for _ in range(args.warmup):
            capture.read()
        frames = []
        while len(frames) < args.frames:
            ok, frame = capture.read()
            if ok and frame is not None:
                frames.append(frame)
        median = np.median(np.stack(frames, axis=0), axis=0).astype(np.uint8)
        output = Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        if not cv2.imwrite(str(output), median):
            raise RuntimeError(f"Cannot save {output}")
        deviations = [float(np.mean(cv2.absdiff(frame, median))) for frame in frames]
        print(f"Saved {output}: frames={len(frames)} mean_residual={np.mean(deviations):.2f} "
              f"max_residual={np.max(deviations):.2f}")
    finally:
        capture.release()


if __name__ == "__main__":
    main()
