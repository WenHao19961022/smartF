cmake_minimum_required(VERSION 3.10)
project(SmartFridge)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 启用线程支持（解决 pthread_create 错误）
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# 添加第三方库路径
set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/third_part/opencv;${CMAKE_PREFIX_PATH}")

# 寻找 OpenCV
find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    message(STATUS "OpenCV library status:")
    message(STATUS "    version: ${OpenCV_VERSION}")
    message(STATUS "    libraries: ${OpenCV_LIBS}")
    message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")
else()
    message(FATAL_ERROR "OpenCV not found, please install OpenCV first.")
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 添加第三方子目录
if(EXISTS ${CMAKE_SOURCE_DIR}/third_part/opencv/CMakeLists.txt)
    add_subdirectory(third_part/opencv)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/third_part/mqtt/CMakeLists.txt)
    add_subdirectory(third_part/mqtt)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/third_part/tensorrt/CMakeLists.txt)
    add_subdirectory(third_part/tensorrt)
endif()

# 添加模块子目录
add_subdirectory(stm32_message_recever)
add_subdirectory(cv_model)
add_subdirectory(mqtt_message_sender)
add_subdirectory(core)