#!/usr/bin/env python3
import argparse
import glob
import os
import time

import cv2
import numpy as np


CLASS_NAMES = [
    "fresh_apple",
    "fresh_banana",
    "fresh_orange",
    "rotten_apple",
    "rotten_banana",
    "rotten_orange",
]


def iter_images(path):
    if os.path.isdir(path):
        patterns = ["*.jpg", "*.jpeg", "*.png", "*.bmp", "*.webp"]
        files = []
        for pattern in patterns:
            files.extend(glob.glob(os.path.join(path, pattern)))
        return sorted(files)
    return [path]


def parse_output(raw, conf_threshold):
    output = np.squeeze(raw)
    if output.ndim != 2:
        raise RuntimeError(f"Unsupported output shape: {raw.shape}")

    if output.shape[0] < output.shape[1]:
        output = output.T

    channels = output.shape[1]
    class_count = min(channels - 4, len(CLASS_NAMES))
    detections = []

    for row in output:
        scores = row[4:4 + class_count]
        class_id = int(np.argmax(scores))
        conf = float(scores[class_id])
        if conf < conf_threshold:
            continue

        fresh_apple = float(scores[0]) if class_count > 0 else 0.0
        rotten_apple = float(scores[3]) if class_count > 3 else 0.0
        detections.append({
            "class_id": class_id,
            "label": CLASS_NAMES[class_id],
            "conf": conf,
            "fresh_apple": fresh_apple,
            "rotten_apple": rotten_apple,
            "cx": float(row[0]),
            "cy": float(row[1]),
            "w": float(row[2]),
            "h": float(row[3]),
        })

    detections.sort(key=lambda item: item["conf"], reverse=True)
    return detections


def infer_image(net, image, conf_threshold):
    blob = cv2.dnn.blobFromImage(
        image,
        scalefactor=1.0 / 255.0,
        size=(640, 640),
        mean=(0, 0, 0),
        swapRB=True,
        crop=False,
    )
    net.setInput(blob)
    output = net.forward()
    return parse_output(output, conf_threshold)


def print_detections(title, detections, topk):
    print(f"\n===== {title} =====")
    if not detections:
        print("No detections above threshold")
        return

    for det in detections[:topk]:
        print(
            f"[{det['class_id']}] {det['label']} "
            f"conf={det['conf']:.6f} "
            f"apple_scores(fresh={det['fresh_apple']:.6f}, "
            f"rotten={det['rotten_apple']:.6f}) "
            f"box(cx={det['cx']:.1f}, cy={det['cy']:.1f}, "
            f"w={det['w']:.1f}, h={det['h']:.1f})"
        )


def run_images(args, net):
    files = iter_images(args.images)
    if not files:
        raise RuntimeError(f"No images found: {args.images}")

    for image_path in files:
        image = cv2.imread(image_path)
        if image is None:
            print(f"Skip unreadable image: {image_path}")
            continue
        detections = infer_image(net, image, args.conf)
        print_detections(image_path, detections, args.topk)


def run_camera(args, net):
    cap = cv2.VideoCapture(args.camera, cv2.CAP_V4L2)
    if not cap.isOpened():
        raise RuntimeError(f"Cannot open camera: {args.camera}")

    print("Press Ctrl+C to stop. Show the rotten fruit to the camera.")
    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                continue
            detections = infer_image(net, frame, args.conf)
            print_detections(f"camera frame {time.strftime('%H:%M:%S')}", detections, args.topk)
            time.sleep(args.interval)
    finally:
        cap.release()


def main():
    parser = argparse.ArgumentParser(description="Test whether the YOLO model can detect rotten fruit.")
    parser.add_argument("--model", default="yolov8s.onnx", help="Path to ONNX model")
    parser.add_argument("--images", help="Image file or directory to test")
    parser.add_argument("--camera", type=int, help="Camera index, for example 0")
    parser.add_argument("--conf", type=float, default=0.25, help="Confidence threshold")
    parser.add_argument("--topk", type=int, default=10, help="Number of detections to print")
    parser.add_argument("--interval", type=float, default=1.0, help="Seconds between camera tests")
    args = parser.parse_args()

    if not args.images and args.camera is None:
        parser.error("Use --images PATH or --camera INDEX")

    net = cv2.dnn.readNet(args.model)
    net.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
    net.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)

    if args.images:
        run_images(args, net)
    else:
        run_camera(args, net)


if __name__ == "__main__":
    main()
