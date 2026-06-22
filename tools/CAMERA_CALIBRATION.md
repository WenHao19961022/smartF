# 电脑直连摄像头定位测试

安装 Python OpenCV 后，在项目根目录执行：

```bash
python3 tools/camera_location_calibration.py --camera 0 \
  --output /tmp/location_calibration.conf \
  --capture /tmp/fridge_frame.jpg
```

保持摄像头在冰箱中的最终安装角度，依次点击有效底板的 `左上 → 右上 → 右下 → 左下` 四个角。工具会生成把梯形底板展开为 `0–255` 平面坐标所需的透视参数。完成后把输出的配置复制到 `config/smartfridge.conf`。

按 `R` 可重新标定，按 `S` 保存当前原图，按 `Q` 退出。

需要同时查看模型识别框时执行：

```bash
python3 cv_model/yolov8/test_rotten_detection.py \
  --model cv_model/yolov8/yolov8s.onnx \
  --camera 0 --interval 0.2 --show
```
