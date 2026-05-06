# CMakeLists.txt (根目录)

cmake_minimum_required(VERSION 3.10)
project(SmartFridge)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 基础依赖查找
find_package(Threads REQUIRED)
find_package(OpenCV REQUIRED)

# 显式寻找 CUDA (Jetson 核心)
# 这能确保全局环境能找到 CUDA 库路径
find_package(CUDA REQUIRED) 

# 2. 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 3. 首先添加所有第三方模块 (定义接口库 3rd_xxx)
add_subdirectory(third_part/opencv)
add_subdirectory(third_part/mqtt)
add_subdirectory(third_part/tensorrt)

# 4. 然后添加业务模块
add_subdirectory(stm32_message_recever)
add_subdirectory(cv_model)
add_subdirectory(mqtt_message_sender)

# 5. 最后添加主程序
add_subdirectory(core)