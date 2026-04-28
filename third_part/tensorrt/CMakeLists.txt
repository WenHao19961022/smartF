# third_part/tensorrt/CMakeLists.txt

# 1. 设置 TensorRT 的安装路径（优先检查环境变量）
if(NOT TENSORRT_ROOT)
    if(EXISTS "/usr/local/TensorRT")
        set(TENSORRT_ROOT "/usr/local/TensorRT")
    elseif(EXISTS "/usr/lib/aarch64-linux-gnu") # Jetson 默认路径
        set(TENSORRT_ROOT "/usr")
    endif()
endif()

# 2. 寻找头文件
find_path(TRT_INCLUDE_DIR NvInfer.h
    PATHS ${TENSORRT_ROOT}/include
    NO_DEFAULT_PATH
)

# 3. 寻找库文件
find_library(TRT_INFER_LIB nvinfer
    PATHS ${TENSORRT_ROOT}/lib ${TENSORRT_ROOT}/lib64
    NO_DEFAULT_PATH
)

# 4. 封装成接口库
if(TRT_INCLUDE_DIR AND TRT_INFER_LIB)
    add_library(3rd_tensorrt INTERFACE)
    target_include_directories(3rd_tensorrt INTERFACE ${TRT_INCLUDE_DIR})
    target_link_libraries(3rd_tensorrt INTERFACE ${TRT_INFER_LIB})
    message(STATUS "Found TensorRT at: ${TENSORRT_ROOT}")
else()
    message(FATAL_ERROR "TensorRT not found! Please set TENSORRT_ROOT")
endif()