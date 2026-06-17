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


def dark_spot_ratio(image, det):
    resized = cv2.resize(image, (640, 640))
    x1 = max(0, int(round(det["cx"] - det["w"] * 0.5)))
    y1 = max(0, int(round(det["cy"] - det["h"] * 0.5)))
    x2 = min(640, int(round(det["cx"] + det["w"] * 0.5)))
    y2 = min(640, int(round(det["cy"] + det["h"] * 0.5)))
    if x2 <= x1 or y2 <= y1 or x2 - x1 < 12 or y2 - y1 < 12:
        return 0.0

    crop = resized[y1:y2, x1:x2]
    hsv = cv2.cvtColor(crop, cv2.COLOR_BGR2HSV)
    dark_mask = cv2.inRange(hsv, (0, 30, 0), (180, 255, 80))
    fruit_color_mask = cv2.inRange(hsv, (0, 35, 40), (180, 255, 255))
    fruit_mask = cv2.bitwise_or(fruit_color_mask, dark_mask)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    dark_mask = cv2.morphologyEx(dark_mask, cv2.MORPH_OPEN, kernel)
    fruit_mask = cv2.morphologyEx(fruit_mask, cv2.MORPH_CLOSE, kernel)
    fruit_pixels = cv2.countNonZero(fruit_mask)
    if fruit_pixels <= 0:
        return 0.0
    return cv2.countNonZero(dark_mask) / fruit_pixels


def to_location_byte(model_coord, model_size=640.0):
    if not np.isfinite(model_coord) or model_size <= 0:
        return 0
    clamped = max(0.0, min(float(model_size), float(model_coord)))
    return int(max(0, min(255, round(clamped * 255.0 / model_size))))


def rect_iou(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    x1 = max(ax, bx)
    y1 = max(ay, by)
    x2 = min(ax + aw, bx + bw)
    y2 = min(ay + ah, by + bh)
    inter = max(0, x2 - x1) * max(0, y2 - y1)
    union = aw * ah + bw * bh - inter
    return inter / union if union > 0 else 0.0


def accept_bag_contour(contour, mask, min_area_ratio, max_area_ratio):
    area = cv2.contourArea(contour)
    frame_area = mask.shape[0] * mask.shape[1]
    if frame_area <= 0:
        return None

    area_ratio = area / frame_area
    if area_ratio < min_area_ratio or area_ratio > max_area_ratio:
        return None

    x, y, w, h = cv2.boundingRect(contour)
    if w < 30 or h < 30:
        return None

    aspect = w / h
    if aspect < 0.25 or aspect > 4.0:
        return None

    fill_ratio = area / (w * h)
    if fill_ratio < 0.12:
        return None

    return (x, y, w, h), fill_ratio


def add_bag_candidate(candidates, candidate):
    for index, existing in enumerate(candidates):
        if rect_iou(existing["rect"], candidate["rect"]) > 0.35:
            if candidate["score"] > existing["score"]:
                candidates[index] = candidate
            return
    candidates.append(candidate)


def detection_rect(det):
    x1 = max(0, int(round(det["cx"] - det["w"] * 0.5)))
    y1 = max(0, int(round(det["cy"] - det["h"] * 0.5)))
    x2 = min(640, int(round(det["cx"] + det["w"] * 0.5)))
    y2 = min(640, int(round(det["cy"] + det["h"] * 0.5)))
    if x2 <= x1 or y2 <= y1:
        return None
    return x1, y1, x2 - x1, y2 - y1


def overlaps_known_fruit(candidate, fruit_rects):
    _, _, cw, ch = candidate
    candidate_area = cw * ch
    if candidate_area <= 0:
        return True

    cx, cy, cw, ch = candidate
    for fruit_rect in fruit_rects:
        fx, fy, fw, fh = fruit_rect
        x1 = max(cx, fx)
        y1 = max(cy, fy)
        x2 = min(cx + cw, fx + fw)
        y2 = min(cy + ch, fy + fh)
        inter_area = max(0, x2 - x1) * max(0, y2 - y1)
        if inter_area <= 0:
            continue
        candidate_overlap = inter_area / candidate_area
        if rect_iou(candidate, fruit_rect) > 0.25 or candidate_overlap > 0.55:
            return True
    return False


def detect_plastic_bags_opencv(image, detections, args):
    if args.no_bag:
        return []

    resized = cv2.resize(image, (640, 640))
    hsv = cv2.cvtColor(resized, cv2.COLOR_BGR2HSV)
    candidates = []
    fruit_rects = [rect for rect in (detection_rect(det) for det in detections) if rect is not None]

    red_low = cv2.inRange(hsv, (0, args.bag_red_s_min, args.bag_red_v_min), (12, 255, 255))
    red_high = cv2.inRange(hsv, (168, args.bag_red_s_min, args.bag_red_v_min), (180, 255, 255))
    red_mask = cv2.bitwise_or(red_low, red_high)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    red_mask = cv2.morphologyEx(red_mask, cv2.MORPH_OPEN, kernel)
    red_mask = cv2.morphologyEx(red_mask, cv2.MORPH_CLOSE, kernel)

    contours, _ = cv2.findContours(red_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    for contour in contours:
        accepted = accept_bag_contour(
            contour, red_mask, args.bag_red_min_area, args.bag_max_area
        )
        if accepted is None:
            continue
        rect, fill_ratio = accepted
        if overlaps_known_fruit(rect, fruit_rects):
            continue
        add_bag_candidate(candidates, {"rect": rect, "score": fill_ratio, "color": "red"})

    white_mask = cv2.inRange(hsv, (0, 0, args.bag_white_v_min), (180, args.bag_white_s_max, 255))
    white_mask = cv2.morphologyEx(white_mask, cv2.MORPH_OPEN, kernel)
    white_mask = cv2.morphologyEx(white_mask, cv2.MORPH_CLOSE, kernel)

    gray = cv2.cvtColor(resized, cv2.COLOR_BGR2GRAY)
    edges = cv2.Canny(gray, 60, 140)
    contours, _ = cv2.findContours(white_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    for contour in contours:
        accepted = accept_bag_contour(
            contour, white_mask, args.bag_white_min_area, args.bag_max_area
        )
        if accepted is None:
            continue
        rect, fill_ratio = accepted
        if overlaps_known_fruit(rect, fruit_rects):
            continue
        contour_mask = np.zeros(white_mask.shape, dtype=np.uint8)
        cv2.drawContours(contour_mask, [contour], 0, 255, cv2.FILLED)
        contour_edges = cv2.bitwise_and(edges, contour_mask)
        area_pixels = cv2.countNonZero(contour_mask)
        edge_ratio = cv2.countNonZero(contour_edges) / area_pixels if area_pixels > 0 else 0.0
        if edge_ratio < args.bag_white_edge_min:
            continue
        add_bag_candidate(
            candidates,
            {"rect": rect, "score": edge_ratio + fill_ratio * 0.1, "color": "white"},
        )

    bags = []
    for candidate in candidates:
        x, y, w, h = candidate["rect"]
        bags.append({
            "color": candidate["color"],
            "score": candidate["score"],
            "location_x": to_location_byte(x + w * 0.5),
            "location_y": to_location_byte(y + h * 0.5),
            "rect": candidate["rect"],
        })
    bags.sort(key=lambda item: item["score"], reverse=True)
    return bags


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


def infer_image(net, image, args):
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
    detections = parse_output(output, args.conf)
    for det in detections:
        det["dark_ratio"] = dark_spot_ratio(image, det)
        det["spot_rotten"] = det["dark_ratio"] >= 0.15
    bags = detect_plastic_bags_opencv(image, detections, args)
    return detections, bags


def print_detections(title, detections, bags, topk):
    print(f"\n===== {title} =====")
    if not detections and not bags:
        print("No detections above threshold")
        return

    for det in detections[:topk]:
        print(
            f"[{det['class_id']}] {det['label']} "
            f"conf={det['conf']:.6f} "
            f"apple_scores(fresh={det['fresh_apple']:.6f}, "
            f"rotten={det['rotten_apple']:.6f}) "
            f"dark_ratio={det.get('dark_ratio', 0.0):.3f} "
            f"spot_rotten={det.get('spot_rotten', False)} "
            f"box(cx={det['cx']:.1f}, cy={det['cy']:.1f}, "
            f"w={det['w']:.1f}, h={det['h']:.1f})"
        )

    for bag in bags[:topk]:
        x, y, w, h = bag["rect"]
        print(
            f"[7] plastic_bag detector=opencv color={bag['color']} "
            f"score={bag['score']:.6f} "
            f"pos=({bag['location_x']},{bag['location_y']}) "
            f"box(x={x}, y={y}, w={w}, h={h})"
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
        detections, bags = infer_image(net, image, args)
        print_detections(image_path, detections, bags, args.topk)


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
            detections, bags = infer_image(net, frame, args)
            print_detections(f"camera frame {time.strftime('%H:%M:%S')}", detections, bags, args.topk)
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
    parser.add_argument("--no-bag", action="store_true", help="Disable OpenCV red/white bag detection")
    parser.add_argument("--bag-red-s-min", type=int, default=70, help="Minimum saturation for red bag")
    parser.add_argument("--bag-red-v-min", type=int, default=50, help="Minimum brightness for red bag")
    parser.add_argument("--bag-red-min-area", type=float, default=0.01, help="Minimum red bag area ratio")
    parser.add_argument("--bag-white-min-area", type=float, default=0.02, help="Minimum white bag area ratio")
    parser.add_argument("--bag-max-area", type=float, default=0.45, help="Maximum bag area ratio")
    parser.add_argument("--bag-white-s-max", type=int, default=55, help="Maximum saturation for white bag")
    parser.add_argument("--bag-white-v-min", type=int, default=135, help="Minimum brightness for white bag")
    parser.add_argument("--bag-white-edge-min", type=float, default=0.015, help="Minimum edge density for white bag")
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
