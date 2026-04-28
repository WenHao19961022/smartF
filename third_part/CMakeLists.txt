# third_part/CMakeLists.txt

# 按照依赖顺序调用子目录
add_subdirectory(mqtt)     # 处理 MQTT C 和 C++
add_subdirectory(opencv)   # 处理 OpenCV 寻找逻辑
add_subdirectory(tensorrt) # 处理 TensorRT 寻找逻辑