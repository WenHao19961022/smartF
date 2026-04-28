cmake_minimum_required(VERSION 3.10)
project(SmartFridge)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加第三方库路径（如果有自定义构建）
set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/third_part/opencv;${CMAKE_PREFIX_PATH}")

# 寻找外部依赖
find_package(OpenCV REQUIRED)

# 添加第三方子目录（如果需要构建）
add_subdirectory(third_part/opencv)
add_subdirectory(third_part/mqtt)
add_subdirectory(third_part/tensorrt)

# 添加模块子目录
add_subdirectory(stm32_message_recever)
add_subdirectory(cv_model)
add_subdirectory(mqtt_message_sender)
add_subdirectory(core)