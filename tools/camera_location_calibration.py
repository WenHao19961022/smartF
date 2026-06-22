#!/usr/bin/env python3
"""Interactive calibration for the smart-fridge 0..255 location coordinates."""

import argparse
from pathlib import Path

import cv2


STEPS = (
    ("TOP_LEFT", "tl"),
    ("TOP_RIGHT", "tr"),
    ("BOTTOM_RIGHT", "br"),
    ("BOTTOM_LEFT", "bl"),
)


def main():
    parser = argparse.ArgumentParser(
        description="Click the usable bottom-board corners: TL, TR, BR and BL."
    )
    parser.add_argument("--camera", type=int, default=0, help="OpenCV camera index")
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=480)
    parser.add_argument("--output", help="Optional path for the generated config snippet")
    parser.add_argument("--capture", help="Optional path used by the S key to save a raw frame")
    args = parser.parse_args()

    cap = cv2.VideoCapture(args.camera)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
    if not cap.isOpened():
        raise RuntimeError(f"Cannot open camera index {args.camera}")

    window = "Smart Fridge Location Calibration"
    cv2.namedWindow(window, cv2.WINDOW_NORMAL)
    points = []
    latest_frame = None

    def reset():
        points.clear()
        print("Calibration reset")

    def config_text():
        values = {key: value for (_, key), value in zip(STEPS, points)}
        return "\n".join((
            "cv.location_perspective_enable = 1",
            f"cv.location_tl_x = {values['tl'][0]:.6f}",
            f"cv.location_tl_y = {values['tl'][1]:.6f}",
            f"cv.location_tr_x = {values['tr'][0]:.6f}",
            f"cv.location_tr_y = {values['tr'][1]:.6f}",
            f"cv.location_br_x = {values['br'][0]:.6f}",
            f"cv.location_br_y = {values['br'][1]:.6f}",
            f"cv.location_bl_x = {values['bl'][0]:.6f}",
            f"cv.location_bl_y = {values['bl'][1]:.6f}",
        )) + "\n"

    def on_mouse(event, x, y, _flags, _userdata):
        if event != cv2.EVENT_LBUTTONDOWN or latest_frame is None or len(points) >= len(STEPS):
            return
        height, width = latest_frame.shape[:2]
        step_name, key = STEPS[len(points)]
        value = (x / width, y / height)
        points.append(value)
        model_x = x * 640.0 / width
        model_y = y * 640.0 / height
        location_x = round(x * 255.0 / width)
        location_y = round(y * 255.0 / height)
        print(
            f"{step_name}: pixel=({x},{y}) model=({model_x:.1f},{model_y:.1f}) "
            f"raw_location=({location_x},{location_y}) {key}=({value[0]:.6f},{value[1]:.6f})"
        )
        if len(points) == len(STEPS):
            text = config_text()
            print("\nCalibration complete. Copy into config/smartfridge.conf:\n" + text)
            if args.output:
                Path(args.output).write_text(text, encoding="utf-8")
                print(f"Saved config snippet: {args.output}")

    cv2.setMouseCallback(window, on_mouse)
    print("Left-click bottom-board corners in this order: TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT.")
    print("Keys: R reset, S save raw frame, Q/ESC quit.")

    try:
        while True:
            ok, frame = cap.read()
            if not ok or frame is None:
                continue
            latest_frame = frame
            display = frame.copy()
            next_step = STEPS[len(points)][0] if len(points) < len(STEPS) else "DONE"
            cv2.putText(display, f"Click: {next_step}", (16, 32),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2, cv2.LINE_AA)
            cv2.putText(display, "R reset | S capture | Q quit", (16, 62),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 255, 255), 1, cv2.LINE_AA)
            for index, value in enumerate(points):
                name, key = STEPS[index]
                cv2.putText(display, f"{name}: ({value[0]:.4f},{value[1]:.4f})", (16, 92 + index * 25),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 220, 0), 1, cv2.LINE_AA)
            cv2.imshow(window, display)
            key = cv2.waitKey(20) & 0xFF
            if key in (ord("q"), 27):
                break
            if key == ord("r"):
                reset()
            if key == ord("s"):
                path = args.capture or "camera_calibration_frame.jpg"
                cv2.imwrite(path, frame)
                print(f"Saved raw frame: {path}")
    finally:
        cap.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
