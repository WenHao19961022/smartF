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


def final_label(det):
    class_id = det["class_id"]
    if det.get("spot_rotten", False):
        if class_id in (0, 3):
            return "rotten_apple"
        if class_id in (1, 4):
            return "rotten_banana"
        if class_id in (2, 5):
            return "rotten_orange"
    return det["label"]


def dark_spot_ratio(image, det, args):
    resized = cv2.resize(image, (640, 640))
    x1 = max(0, int(round(det["cx"] - det["w"] * 0.5)))
    y1 = max(0, int(round(det["cy"] - det["h"] * 0.5)))
    x2 = min(640, int(round(det["cx"] + det["w"] * 0.5)))
    y2 = min(640, int(round(det["cy"] + det["h"] * 0.5)))
    if x2 <= x1 or y2 <= y1 or x2 - x1 < 12 or y2 - y1 < 12:
        return 0.0

    crop = resized[y1:y2, x1:x2]
    hsv = cv2.cvtColor(crop, cv2.COLOR_BGR2HSV)
    dark_v = args.rotten_dark_v_threshold
    dark_s = args.rotten_dark_s_threshold
    if det["class_id"] in (2, 5):
        dark_v = args.rotten_dark_v_threshold_orange
        dark_s = args.rotten_dark_s_threshold_orange
    dark_mask = cv2.inRange(hsv, (0, dark_s, 0), (180, 255, dark_v))
    fruit_color_mask = cv2.inRange(hsv, (0, 35, 40), (180, 255, 255))
    fruit_mask = cv2.bitwise_or(fruit_color_mask, dark_mask)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    dark_mask = cv2.morphologyEx(dark_mask, cv2.MORPH_OPEN, kernel)
    fruit_mask = cv2.morphologyEx(fruit_mask, cv2.MORPH_CLOSE, kernel)
    fruit_pixels = cv2.countNonZero(fruit_mask)
    if fruit_pixels <= 0:
        return 0.0
    return cv2.countNonZero(dark_mask) / fruit_pixels


def orange_peel_ratio(image, det):
    resized = cv2.resize(image, (640, 640))
    x1 = max(0, int(round(det["cx"] - det["w"] * 0.5)))
    y1 = max(0, int(round(det["cy"] - det["h"] * 0.5)))
    x2 = min(640, int(round(det["cx"] + det["w"] * 0.5)))
    y2 = min(640, int(round(det["cy"] + det["h"] * 0.5)))
    if x2 <= x1 or y2 <= y1 or x2 - x1 < 12 or y2 - y1 < 12:
        return 0.0

    crop = resized[y1:y2, x1:x2]
    hsv = cv2.cvtColor(crop, cv2.COLOR_BGR2HSV)
    orange_mask = cv2.inRange(hsv, (5, 60, 50), (35, 255, 255))
    fruit_mask = cv2.inRange(hsv, (0, 35, 40), (180, 255, 255))
    dark_mask = cv2.inRange(hsv, (0, 30, 0), (180, 255, 80))
    fruit_mask = cv2.bitwise_or(fruit_mask, dark_mask)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    orange_mask = cv2.morphologyEx(orange_mask, cv2.MORPH_OPEN, kernel)
    fruit_mask = cv2.morphologyEx(fruit_mask, cv2.MORPH_CLOSE, kernel)
    fruit_pixels = cv2.countNonZero(fruit_mask)
    if fruit_pixels <= 0:
        return 0.0
    return cv2.countNonZero(orange_mask) / fruit_pixels


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


def fruit_type_key(det):
    class_id = det["class_id"]
    if class_id in (0, 3):
        return "apple"
    if class_id in (1, 4):
        return "banana"
    if class_id in (2, 5):
        return "orange"
    return str(class_id)


def nms_detections(detections, iou_threshold):
    kept = []
    suppressed = [False] * len(detections)
    for i, det in enumerate(detections):
        if suppressed[i]:
            continue
        kept.append(det)
        rect = detection_rect(det)
        if rect is None:
            continue
        det_key = fruit_type_key(det)
        for j in range(i + 1, len(detections)):
            if suppressed[j] or fruit_type_key(detections[j]) != det_key:
                continue
            other_rect = detection_rect(detections[j])
            if other_rect is not None and rect_iou(rect, other_rect) > iou_threshold:
                suppressed[j] = True
    return kept


def detect_oranges_opencv(image, detections, args):
    if not args.orange_opencv:
        return []

    resized = cv2.resize(image, (640, 640))
    hsv = cv2.cvtColor(resized, cv2.COLOR_BGR2HSV)
    orange_mask = cv2.inRange(hsv, (5, 60, 50), (35, 255, 255))
    orange_mask = cv2.morphologyEx(
        orange_mask, cv2.MORPH_OPEN, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    )
    orange_mask = cv2.morphologyEx(
        orange_mask, cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 9))
    )

    fruit_rects = [rect for rect in (detection_rect(det) for det in detections) if rect is not None]
    contours, _ = cv2.findContours(orange_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    frame_area = 640.0 * 640.0
    oranges = []
    for contour in contours:
        area = cv2.contourArea(contour)
        area_ratio = area / frame_area
        if area_ratio < args.orange_opencv_min_area or area_ratio > args.orange_opencv_max_area:
            continue

        x, y, w, h = cv2.boundingRect(contour)
        if w < args.min_fruit_box_side or h < args.min_fruit_box_side:
            continue
        aspect = w / h
        if aspect < args.orange_opencv_min_aspect or aspect > args.orange_opencv_max_aspect:
            continue

        perimeter = cv2.arcLength(contour, True)
        circularity = 4.0 * np.pi * area / (perimeter * perimeter) if perimeter > 0 else 0.0
        if circularity < args.orange_opencv_min_circularity:
            continue

        rect = (x, y, w, h)
        if overlaps_known_fruit(rect, fruit_rects, args.bag_fruit_overlap_max):
            continue

        rect_area = max(1, w * h)
        orange_coverage = cv2.countNonZero(orange_mask[y:y + h, x:x + w]) / rect_area
        if orange_coverage < args.orange_opencv_min_coverage:
            continue

        det = {
            "class_id": 2,
            "label": "fresh_orange",
            "conf": min(0.99, max(0.25, orange_coverage * circularity)),
            "fresh_apple": 0.0,
            "rotten_apple": 0.0,
            "fresh_orange": min(0.99, orange_coverage),
            "rotten_orange": 0.0,
            "cx": x + w * 0.5,
            "cy": y + h * 0.5,
            "w": float(w),
            "h": float(h),
            "orange_ratio": orange_coverage,
            "color_reclass": "opencv_orange",
        }
        det["dark_ratio"] = dark_spot_ratio(image, det, args)
        det["spot_rotten"] = det["dark_ratio"] >= args.rotten_dark_ratio_threshold_orange
        det["final_label"] = final_label(det)
        oranges.append(det)
        fruit_rects.append(rect)

    return oranges


def overlaps_known_fruit(candidate, fruit_rects, max_combined_overlap=0.35):
    _, _, cw, ch = candidate
    candidate_area = cw * ch
    if candidate_area <= 0:
        return True

    cx, cy, cw, ch = candidate
    combined_fruit_overlap = 0
    for fruit_rect in fruit_rects:
        fx, fy, fw, fh = fruit_rect
        x1 = max(cx, fx)
        y1 = max(cy, fy)
        x2 = min(cx + cw, fx + fw)
        y2 = min(cy + ch, fy + fh)
        inter_area = max(0, x2 - x1) * max(0, y2 - y1)
        if inter_area <= 0:
            continue
        combined_fruit_overlap += inter_area
        candidate_overlap = inter_area / candidate_area
        fruit_area = fw * fh
        similar_size_to_fruit = candidate_area <= fruit_area * 1.25
        if candidate_overlap > 0.55 or (similar_size_to_fruit and rect_iou(candidate, fruit_rect) > 0.25):
            return True
    if min(combined_fruit_overlap, candidate_area) / candidate_area > max_combined_overlap:
        return True
    return False


def clamp_rect(rect, width, height):
    x, y, w, h = rect
    x1 = max(0, x)
    y1 = max(0, y)
    x2 = min(width, x + w)
    y2 = min(height, y + h)
    if x2 <= x1 or y2 <= y1:
        return None
    return x1, y1, x2 - x1, y2 - y1


def expand_rect(rect, margin, width, height):
    x, y, w, h = rect
    return clamp_rect((x - margin, y - margin, w + margin * 2, h + margin * 2), width, height)


def touches_image_border(rect, width, height, margin_ratio):
    x, y, w, h = rect
    border_margin_x = int(width * margin_ratio)
    border_margin_y = int(height * margin_ratio)
    return (
        x <= border_margin_x
        or y <= border_margin_y
        or x + w >= width - border_margin_x
        or y + h >= height - border_margin_y
    )


def warm_fruit_coverage(hsv, rect):
    x, y, w, h = rect
    hsv_roi = hsv[y:y + h, x:x + w]
    warm_mask = cv2.inRange(hsv_roi, (0, 50, 50), (45, 255, 255))
    red_mask = cv2.inRange(hsv_roi, (160, 50, 50), (180, 255, 255))
    warm_mask = cv2.bitwise_or(warm_mask, red_mask)
    return cv2.countNonZero(warm_mask) / max(1, w * h)


def reject_warm_fruit_as_white_bag(rect, hsv, white_coverage, args):
    warm_coverage = warm_fruit_coverage(hsv, rect)
    return (
        warm_coverage > args.bag_white_warm_fruit_ratio_max
        and warm_coverage > white_coverage * 1.2
    )


def accept_white_bag_rect(rect, white_mask, white_support_mask, edges, gray, hsv, args):
    rect = clamp_rect(rect, white_mask.shape[1], white_mask.shape[0])
    if rect is None:
        return None

    x, y, w, h = rect
    if w < 30 or h < 30:
        return None

    frame_area = white_mask.shape[0] * white_mask.shape[1]
    rect_area = w * h
    area_ratio = rect_area / frame_area
    if area_ratio < args.bag_white_min_area or area_ratio > args.bag_max_area:
        return None

    aspect = w / h
    if aspect < 0.25 or aspect > 4.0:
        return None

    if args.bag_white_reject_border_touch and touches_image_border(
        rect, white_mask.shape[1], white_mask.shape[0], args.bag_white_border_margin
    ):
        return None

    white_roi = white_mask[y:y + h, x:x + w]
    white_coverage = cv2.countNonZero(white_roi) / rect_area
    if white_coverage < args.bag_white_coverage_min:
        return None
    if reject_warm_fruit_as_white_bag(rect, hsv, white_coverage, args):
        return None

    edge_support_roi = cv2.bitwise_and(edges[y:y + h, x:x + w], white_support_mask[y:y + h, x:x + w])
    edge_ratio = cv2.countNonZero(edge_support_roi) / rect_area
    if edge_ratio < args.bag_white_edge_min:
        return None

    inner = clamp_rect((x + 8, y + 8, w - 16, h - 16), white_mask.shape[1], white_mask.shape[0])
    inner_edge_ratio = 0.0
    if inner is not None:
        ix, iy, iw, ih = inner
        inner_edges = cv2.bitwise_and(edges[iy:iy + ih, ix:ix + iw], white_support_mask[iy:iy + ih, ix:ix + iw])
        inner_edge_ratio = cv2.countNonZero(inner_edges) / (iw * ih)

    mean_gray, std_gray = cv2.meanStdDev(gray[y:y + h, x:x + w], mask=white_roi)
    gray_stddev = float(std_gray[0][0])

    border_contrast = 0.0
    outer = expand_rect(rect, 14, white_mask.shape[1], white_mask.shape[0])
    if outer is not None:
        ox, oy, ow, oh = outer
        if ow * oh > rect_area:
            ring_mask = np.full((oh, ow), 255, dtype=np.uint8)
            ring_mask[y - oy:y - oy + h, x - ox:x - ox + w] = 0
            outer_mean = cv2.mean(gray[oy:oy + oh, ox:ox + ow], mask=ring_mask)[0]
            border_contrast = abs(float(mean_gray[0][0]) - outer_mean)

    hsv_roi = hsv[y:y + h, x:x + w]
    glare_mask = cv2.inRange(hsv_roi[:, :, 2], 245, 255)
    low_saturation_mask = cv2.inRange(hsv_roi[:, :, 1], 0, 35)
    glare_mask = cv2.bitwise_and(glare_mask, low_saturation_mask)
    glare_ratio = cv2.countNonZero(glare_mask) / rect_area

    top_lighting_area = y < white_mask.shape[0] * args.bag_ignore_top_ratio
    has_internal_texture = (
        inner_edge_ratio >= args.bag_white_inner_edge_min
        or gray_stddev >= args.bag_white_stddev_min
    )
    has_object_boundary = (
        border_contrast >= args.bag_white_border_contrast_min
        or edge_ratio >= args.bag_white_edge_min * 1.6
    )

    if top_lighting_area and not has_internal_texture:
        return None
    if (
        glare_ratio > args.bag_white_glare_ratio_max
        and inner_edge_ratio < args.bag_white_inner_edge_min
        and gray_stddev < args.bag_white_stddev_min * 1.5
    ):
        return None
    if not has_internal_texture and not has_object_boundary:
        return None

    score = (
        edge_ratio
        + inner_edge_ratio * 1.5
        + gray_stddev * 0.002
        + border_contrast * 0.001
        + white_coverage * 0.05
    )
    return rect, score


def add_background_bag_candidates(image, red_mask, white_mask, edges, hsv, fruit_rects, candidates, args):
    if not args.background:
        return
    background = cv2.imread(args.background)
    if background is None:
        print(f"Skip background subtraction; cannot read: {args.background}")
        return

    background = cv2.resize(background, (640, 640))
    diff = cv2.absdiff(image, background)
    gray_diff = cv2.cvtColor(diff, cv2.COLOR_BGR2GRAY)
    fg_mask = cv2.threshold(gray_diff, args.bag_bg_diff_threshold, 255, cv2.THRESH_BINARY)[1]
    fg_mask = cv2.morphologyEx(
        fg_mask, cv2.MORPH_OPEN, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    )
    close_kernel = max(3, int(args.bag_bg_close_kernel) | 1)
    fg_mask = cv2.morphologyEx(
        fg_mask, cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_RECT, (close_kernel, close_kernel))
    )

    contours, _ = cv2.findContours(fg_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    frame_area = fg_mask.shape[0] * fg_mask.shape[1]
    for contour in contours:
        contour_area = cv2.contourArea(contour)
        area_ratio = contour_area / frame_area
        if area_ratio < args.bag_bg_min_area or area_ratio > args.bag_bg_max_area:
            continue

        rect = cv2.boundingRect(contour)
        x, y, w, h = rect
        if w < 30 or h < 30:
            continue
        aspect = w / h
        if aspect < 0.25 or aspect > 4.0:
            continue
        if touches_image_border(rect, fg_mask.shape[1], fg_mask.shape[0], args.bag_bg_border_margin):
            continue
        if overlaps_known_fruit(rect, fruit_rects, args.bag_fruit_overlap_max):
            continue

        rect_area = w * h
        white_coverage = cv2.countNonZero(white_mask[y:y + h, x:x + w]) / rect_area
        red_coverage = cv2.countNonZero(red_mask[y:y + h, x:x + w]) / rect_area
        edge_ratio = cv2.countNonZero(edges[y:y + h, x:x + w]) / rect_area
        if (
            white_coverage < args.bag_bg_white_coverage_min
            and red_coverage < args.bag_bg_red_coverage_min
            and edge_ratio < args.bag_bg_edge_min
        ):
            continue

        is_white_candidate = red_coverage <= white_coverage
        if is_white_candidate and reject_warm_fruit_as_white_bag(rect, hsv, white_coverage, args):
            continue
        color = "white" if is_white_candidate else "red"
        score = area_ratio + white_coverage * 0.2 + red_coverage * 0.4 + edge_ratio
        add_bag_candidate(candidates, {"rect": rect, "score": score, "color": color})


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
        if args.bag_red_reject_border_touch and touches_image_border(
            rect, red_mask.shape[1], red_mask.shape[0], args.bag_red_border_margin
        ):
            continue
        if overlaps_known_fruit(rect, fruit_rects, args.bag_fruit_overlap_max):
            continue
        add_bag_candidate(candidates, {"rect": rect, "score": fill_ratio, "color": "red"})

    white_mask = cv2.inRange(hsv, (0, 0, args.bag_white_v_min), (180, args.bag_white_s_max, 255))
    white_mask = cv2.morphologyEx(white_mask, cv2.MORPH_OPEN, kernel)
    white_mask = cv2.morphologyEx(white_mask, cv2.MORPH_CLOSE, kernel)

    gray = cv2.cvtColor(resized, cv2.COLOR_BGR2GRAY)
    edges = cv2.Canny(gray, 60, 140)
    white_support_mask = cv2.dilate(white_mask, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7)))

    add_background_bag_candidates(resized, red_mask, white_mask, edges, hsv, fruit_rects, candidates, args)

    contours, _ = cv2.findContours(white_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    for contour in contours:
        accepted = accept_bag_contour(
            contour, white_mask, args.bag_white_min_area, args.bag_max_area
        )
        if accepted is None:
            continue
        rect, fill_ratio = accepted
        if overlaps_known_fruit(rect, fruit_rects, args.bag_fruit_overlap_max):
            continue
        accepted_rect = accept_white_bag_rect(rect, white_mask, white_support_mask, edges, gray, hsv, args)
        if accepted_rect is None:
            continue
        rect, score = accepted_rect
        add_bag_candidate(candidates, {"rect": rect, "score": score + fill_ratio * 0.05, "color": "white"})

    white_edges = cv2.bitwise_and(edges, white_support_mask)
    white_edges = cv2.dilate(white_edges, cv2.getStructuringElement(cv2.MORPH_RECT, (5, 5)))
    white_edges = cv2.morphologyEx(
        white_edges, cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_RECT, (11, 11))
    )
    contours, _ = cv2.findContours(white_edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    for contour in contours:
        rect = cv2.boundingRect(contour)
        if overlaps_known_fruit(rect, fruit_rects, args.bag_fruit_overlap_max):
            continue
        accepted_rect = accept_white_bag_rect(rect, white_mask, white_support_mask, edges, gray, hsv, args)
        if accepted_rect is None:
            continue
        rect, score = accepted_rect
        add_bag_candidate(candidates, {"rect": rect, "score": score, "color": "white"})

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
        fresh_orange = float(scores[2]) if class_count > 2 else 0.0
        rotten_orange = float(scores[5]) if class_count > 5 else 0.0
        detections.append({
            "class_id": class_id,
            "label": CLASS_NAMES[class_id],
            "conf": conf,
            "fresh_apple": fresh_apple,
            "rotten_apple": rotten_apple,
            "fresh_orange": fresh_orange,
            "rotten_orange": rotten_orange,
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
    detections = [
        det for det in detections
        if (
            det["w"] * det["h"] / (640.0 * 640.0) >= args.min_fruit_box_area_ratio
            and det["w"] >= args.min_fruit_box_side
            and det["h"] >= args.min_fruit_box_side
        )
    ]
    for det in detections:
        det["orange_ratio"] = orange_peel_ratio(image, det)
        score_backed_orange = (
            max(det.get("fresh_orange", 0.0), det.get("rotten_orange", 0.0)) >= args.orange_reclass_score_min
            and det["orange_ratio"] >= args.orange_reclass_ratio_min
        )
        strong_color_orange = det["orange_ratio"] >= args.orange_reclass_strong_ratio_min
        if det["class_id"] == 0 and args.orange_color_reclass and (score_backed_orange or strong_color_orange):
            det["class_id"] = 2 if det.get("fresh_orange", 0.0) >= det.get("rotten_orange", 0.0) else 5
            det["label"] = CLASS_NAMES[det["class_id"]]
            det["color_reclass"] = "apple_to_orange"
        det["dark_ratio"] = dark_spot_ratio(image, det, args)
        rotten_threshold = args.rotten_dark_ratio_threshold
        if det["class_id"] in (2, 5):
            rotten_threshold = args.rotten_dark_ratio_threshold_orange
        elif det["class_id"] in (1, 4):
            rotten_threshold = args.rotten_dark_ratio_threshold_banana
        det["spot_rotten"] = det["dark_ratio"] >= rotten_threshold
        det["final_label"] = final_label(det)
    detections = nms_detections(detections, args.nms)
    detections.extend(detect_oranges_opencv(image, detections, args))
    detections.sort(key=lambda item: item["conf"], reverse=True)
    detections = nms_detections(detections, args.nms)
    bags = detect_plastic_bags_opencv(image, detections, args)
    return detections, bags


def print_detections(title, detections, bags, topk):
    print(f"\n===== {title} =====")
    if not detections and not bags:
        print("No detections above threshold")
        return

    for det in detections[:topk]:
        print(
            f"[{det['class_id']}] {det.get('final_label', det['label'])} "
            f"conf={det['conf']:.6f} "
            f"apple_scores(fresh={det['fresh_apple']:.6f}, "
            f"rotten={det['rotten_apple']:.6f}) "
            f"orange_scores(fresh={det.get('fresh_orange', 0.0):.6f}, "
            f"rotten={det.get('rotten_orange', 0.0):.6f}) "
            f"orange_ratio={det.get('orange_ratio', 0.0):.3f} "
            f"reclass={det.get('color_reclass', '')} "
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


def safe_debug_name(title):
    name = "".join(ch if ch.isalnum() else "_" for ch in title)
    return name.strip("_")[-120:] or "frame"


def scale_rect_to_image(rect, image):
    x, y, w, h = rect
    image_h, image_w = image.shape[:2]
    scale_x = image_w / 640.0
    scale_y = image_h / 640.0
    x1 = int(round(x * scale_x))
    y1 = int(round(y * scale_y))
    x2 = int(round((x + w) * scale_x))
    y2 = int(round((y + h) * scale_y))
    return x1, y1, max(1, x2 - x1), max(1, y2 - y1)


def draw_label(image, x, y, text, color):
    baseline = 0
    font = cv2.FONT_HERSHEY_SIMPLEX
    scale = 0.45
    thickness = 1
    (tw, th), baseline = cv2.getTextSize(text, font, scale, thickness)
    y0 = max(0, y - th - baseline - 4)
    cv2.rectangle(image, (x, y0), (min(image.shape[1] - 1, x + tw + 4), y0 + th + baseline + 4), color, -1)
    cv2.putText(image, text, (x + 2, y0 + th + 1), font, scale, (0, 0, 0), thickness, cv2.LINE_AA)


def annotate_image(image, detections, bags, topk):
    annotated = image.copy()
    for det in detections[:topk]:
        rect = detection_rect(det)
        if rect is None:
            continue
        x, y, w, h = scale_rect_to_image(rect, annotated)
        color = (0, 0, 255) if det.get("spot_rotten", False) or det["class_id"] >= 3 else (0, 180, 0)
        cv2.rectangle(annotated, (x, y), (x + w, y + h), color, 2)
        draw_label(annotated, x, y, f"{det.get('final_label', det['label'])} {det['conf']:.2f}", color)

    for bag in bags[:topk]:
        x, y, w, h = scale_rect_to_image(bag["rect"], annotated)
        color = (0, 0, 255) if bag["color"] == "red" else (255, 255, 255)
        cv2.rectangle(annotated, (x, y), (x + w, y + h), color, 2)
        draw_label(annotated, x, y, f"bag:{bag['color']} {bag['score']:.2f}", color)
    return annotated


def save_debug_outputs(args, title, image, detections, bags):
    if not args.save_debug:
        return
    os.makedirs(args.save_debug, exist_ok=True)
    name = safe_debug_name(title)
    raw_path = os.path.join(args.save_debug, f"{name}_raw.jpg")
    annotated_path = os.path.join(args.save_debug, f"{name}_annotated.jpg")
    cv2.imwrite(raw_path, image)
    cv2.imwrite(annotated_path, annotate_image(image, detections, bags, args.topk))
    print(f"Saved debug images: {raw_path}, {annotated_path}")


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
        save_debug_outputs(args, image_path, image, detections, bags)


def run_camera(args, net):
    cap = cv2.VideoCapture(args.camera, cv2.CAP_V4L2)
    if not cap.isOpened():
        raise RuntimeError(f"Cannot open camera: {args.camera}")

    print("Press Ctrl+C to stop. Show the rotten fruit to the camera.")
    frame_index = 0
    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                continue
            detections, bags = infer_image(net, frame, args)
            frame_index += 1
            title = f"camera_{args.camera}_{time.strftime('%Y%m%d_%H%M%S')}_{frame_index:03d}"
            print_detections(title, detections, bags, args.topk)
            save_debug_outputs(args, title, frame, detections, bags)
            if args.frames > 0 and frame_index >= args.frames:
                break
            time.sleep(args.interval)
    finally:
        cap.release()


def main():
    parser = argparse.ArgumentParser(description="Test whether the YOLO model can detect rotten fruit.")
    parser.add_argument("--model", default="yolov8s.onnx", help="Path to ONNX model")
    parser.add_argument("--images", help="Image file or directory to test")
    parser.add_argument("--camera", type=int, help="Camera index, for example 0")
    parser.add_argument("--conf", type=float, default=0.25, help="Confidence threshold")
    parser.add_argument("--nms", type=float, default=0.4, help="NMS IoU threshold")
    parser.add_argument("--topk", type=int, default=10, help="Number of detections to print")
    parser.add_argument("--interval", type=float, default=1.0, help="Seconds between camera tests")
    parser.add_argument("--frames", type=int, default=0, help="Number of camera frames to process before exiting; 0 means forever")
    parser.add_argument("--save-debug", help="Directory to save raw and annotated debug images")
    parser.add_argument("--background", help="Empty-fridge background image for foreground bag detection")
    parser.add_argument("--min-fruit-box-area-ratio", type=float, default=0.003, help="Drop tiny fruit boxes below this area ratio")
    parser.add_argument("--min-fruit-box-side", type=float, default=36.0, help="Drop fruit boxes with width/height below this size")
    parser.add_argument("--rotten-dark-ratio-threshold", type=float, default=0.15, help="Default dark spot ratio threshold")
    parser.add_argument("--rotten-dark-ratio-threshold-orange", type=float, default=0.07, help="Dark spot ratio threshold for oranges")
    parser.add_argument("--rotten-dark-ratio-threshold-banana", type=float, default=0.15, help="Dark spot ratio threshold for bananas")
    parser.add_argument("--rotten-dark-v-threshold", type=int, default=80, help="Default maximum V for dark spot mask")
    parser.add_argument("--rotten-dark-s-threshold", type=int, default=30, help="Default minimum S for dark spot mask")
    parser.add_argument("--rotten-dark-v-threshold-orange", type=int, default=130, help="Orange maximum V for brown/dark spot mask")
    parser.add_argument("--rotten-dark-s-threshold-orange", type=int, default=30, help="Orange minimum S for brown/dark spot mask")
    parser.add_argument("--orange-color-reclass", action="store_true", default=True, help="Reclass apple-like detections to orange using color")
    parser.add_argument("--orange-reclass-score-min", type=float, default=0.03, help="Minimum orange class score for apple-to-orange reclass")
    parser.add_argument("--orange-reclass-ratio-min", type=float, default=0.45, help="Minimum orange pixel ratio for apple-to-orange reclass")
    parser.add_argument("--orange-reclass-strong-ratio-min", type=float, default=0.80, help="Reclass apple detections to orange when color ratio is very strong")
    parser.add_argument("--orange-opencv", action="store_true", default=True, help="Enable OpenCV color fallback for oranges")
    parser.add_argument("--orange-opencv-min-area", type=float, default=0.008, help="Minimum orange color contour area ratio")
    parser.add_argument("--orange-opencv-max-area", type=float, default=0.18, help="Maximum orange color contour area ratio")
    parser.add_argument("--orange-opencv-min-coverage", type=float, default=0.55, help="Minimum orange coverage inside fallback box")
    parser.add_argument("--orange-opencv-min-circularity", type=float, default=0.35, help="Minimum circularity for fallback orange")
    parser.add_argument("--orange-opencv-min-aspect", type=float, default=0.55, help="Minimum fallback orange bounding-box aspect ratio")
    parser.add_argument("--orange-opencv-max-aspect", type=float, default=1.35, help="Maximum fallback orange bounding-box aspect ratio")
    parser.add_argument("--no-bag", action="store_true", help="Disable OpenCV red/white bag detection")
    parser.add_argument("--bag-red-s-min", type=int, default=70, help="Minimum saturation for red bag")
    parser.add_argument("--bag-red-v-min", type=int, default=50, help="Minimum brightness for red bag")
    parser.add_argument("--bag-red-min-area", type=float, default=0.01, help="Minimum red bag area ratio")
    parser.add_argument("--allow-border-red-bag", dest="bag_red_reject_border_touch", action="store_false", default=True, help="Allow red candidates touching image borders")
    parser.add_argument("--bag-red-border-margin", type=float, default=0.03, help="Image border margin ratio for rejecting red candidates")
    parser.add_argument("--bag-white-min-area", type=float, default=0.02, help="Minimum white bag area ratio")
    parser.add_argument("--bag-max-area", type=float, default=0.45, help="Maximum bag area ratio")
    parser.add_argument("--bag-white-s-max", type=int, default=90, help="Maximum saturation for white bag")
    parser.add_argument("--bag-white-v-min", type=int, default=105, help="Minimum brightness for white bag")
    parser.add_argument("--bag-white-coverage-min", type=float, default=0.18, help="Minimum white coverage in candidate")
    parser.add_argument("--bag-white-edge-min", type=float, default=0.015, help="Minimum edge density for white bag")
    parser.add_argument("--bag-white-inner-edge-min", type=float, default=0.004, help="Minimum internal edge density for white bag")
    parser.add_argument("--bag-white-stddev-min", type=float, default=8.0, help="Minimum gray stddev for white bag")
    parser.add_argument("--bag-white-border-contrast-min", type=float, default=6.0, help="Minimum border contrast for white bag")
    parser.add_argument("--bag-white-glare-ratio-max", type=float, default=0.55, help="Reject smooth overexposed white regions above this ratio")
    parser.add_argument("--bag-white-warm-fruit-ratio-max", type=float, default=0.45, help="Reject white bag candidates dominated by red/orange/yellow fruit color")
    parser.add_argument("--bag-ignore-top-ratio", type=float, default=0.08, help="Reject smooth white artifacts near the top of the frame")
    parser.add_argument("--allow-border-white-bag", dest="bag_white_reject_border_touch", action="store_false", default=True, help="Allow white candidates touching image borders")
    parser.add_argument("--bag-white-border-margin", type=float, default=0.03, help="Image border margin ratio for rejecting white candidates")
    parser.add_argument("--bag-bg-diff-threshold", type=int, default=40, help="Foreground difference threshold for background bag detection")
    parser.add_argument("--bag-bg-min-area", type=float, default=0.01, help="Minimum foreground bag area ratio")
    parser.add_argument("--bag-bg-max-area", type=float, default=0.45, help="Maximum foreground bag area ratio")
    parser.add_argument("--bag-bg-border-margin", type=float, default=0.03, help="Image border margin ratio for foreground bag candidates")
    parser.add_argument("--bag-bg-white-coverage-min", type=float, default=0.15, help="Minimum white coverage for foreground bag")
    parser.add_argument("--bag-bg-red-coverage-min", type=float, default=0.04, help="Minimum red coverage for foreground bag")
    parser.add_argument("--bag-bg-edge-min", type=float, default=0.012, help="Minimum edge density for foreground bag")
    parser.add_argument("--bag-bg-close-kernel", type=int, default=5, help="Morph close kernel size for foreground bag mask")
    parser.add_argument("--bag-fruit-overlap-max", type=float, default=0.35, help="Reject bag candidates mostly covered by fruit detections")
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
