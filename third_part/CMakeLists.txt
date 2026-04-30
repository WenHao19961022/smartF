# third_part/tensorrt/CMakeLists.txt

# 1. 定义搜索路径列表
set(TRT_INCLUDE_PATHS
    /usr/include/aarch64-linux-gnu
    /usr/local/cuda/include
    ${TENSORRT_ROOT}/include
)

set(TRT_LIB_PATHS
    /usr/lib/aarch64-linux-gnu
    /usr/local/cuda/lib64
    ${TENSORRT_ROOT}/lib
)

# 2. 寻找头文件 (去掉 NO_DEFAULT_PATH，增加系统子路径)
find_path(TRT_INCLUDE_DIR NvInfer.h
    PATHS ${TRT_INCLUDE_PATHS}
)

# 3. 寻找库文件
find_library(TRT_INFER_LIB nvinfer
    PATHS ${TRT_LIB_PATHS}
)

# 4. 寻找 ONNX 解析器（推理 YOLOv12 必不可少）
find_library(TRT_ONNX_LIB nvonnxparser
    PATHS ${TRT_LIB_PATHS}
)

# 5. 封装成接口库
if(TRT_INCLUDE_DIR AND TRT_INFER_LIB)
    add_library(3rd_tensorrt INTERFACE)
    
    # 注意：Jetson 的 NvInfer.h 往往依赖 NvInferLegacy.h 等，都在同一目录下
    target_include_directories(3rd_tensorrt INTERFACE ${TRT_INCLUDE_DIR})
    
    # 链接核心库
    target_link_libraries(3rd_tensorrt INTERFACE ${TRT_INFER_LIB})
    
    # 如果找到了 ONNX 解析器也一并链接
    if(TRT_ONNX_LIB)
        target_link_libraries(3rd_tensorrt INTERFACE ${TRT_ONNX_LIB})
        message(STATUS "Found TensorRT ONNX Parser: ${TRT_ONNX_LIB}")
    endif()

    message(STATUS "Found TensorRT Header: ${TRT_INCLUDE_DIR}")
    message(STATUS "Found TensorRT Library: ${TRT_INFER_LIB}")
    set(WITH_TENSORRT ON CACHE BOOL "TensorRT support" FORCE)
else()
    message(WARNING "TensorRT not found in standard Jetson paths. Set TENSORRT_ROOT if using a custom build.")
    add_library(3rd_tensorrt INTERFACE)
    set(WITH_TENSORRT OFF CACHE BOOL "TensorRT support" FORCE)
endif()