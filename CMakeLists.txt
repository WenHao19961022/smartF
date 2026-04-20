cmake_minimum_required(VERSION 3.10)
project(SmartFridge)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 寻找外部依赖（例如 CV 模块肯定需要的 OpenCV）
find_package(OpenCV REQUIRED)

# 按照逻辑顺序添加子目录
add_subdirectory(connect_stm32)
add_subdirectory(cv_model)
add_subdirectory(core)