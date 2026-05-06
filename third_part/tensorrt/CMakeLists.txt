# --- third_part/tensorrt/CMakeLists.txt ---

# 1. 显式设置 CUDA 路径 (针对 Jetson)
set(CUDA_PATH "/usr/local/cuda")
set(CUDA_INCLUDE_DIRS "${CUDA_PATH}/include")
set(CUDA_LIBRARIES "${CUDA_PATH}/lib64/libcudart.so")

# 2. 设置 TensorRT 路径
if(NOT TENSORRT_ROOT)
    if(EXISTS "/usr/lib/aarch64-linux-gnu")
        set(TENSORRT_ROOT "/usr")
    endif()
endif()

find_path(TRT_INCLUDE_DIR NvInfer.h PATHS ${TENSORRT_ROOT}/include NO_DEFAULT_PATH)
find_library(TRT_INFER_LIB nvinfer PATHS ${TENSORRT_ROOT}/lib ${TENSORRT_ROOT}/lib64 NO_DEFAULT_PATH)
# 还需要链接 nvonnxparser 否则 YOLO 无法加载
find_library(TRT_ONNX_LIB nvonnxparser PATHS ${TENSORRT_ROOT}/lib ${TENSORRT_ROOT}/lib64 NO_DEFAULT_PATH)

# 3. 封装接口库
if(TRT_INCLUDE_DIR AND TRT_INFER_LIB)
    add_library(3rd_tensorrt INTERFACE)
    # 注入 TRT + CUDA 的头文件路径
    target_include_directories(3rd_tensorrt INTERFACE 
        ${TRT_INCLUDE_DIR}
        ${CUDA_INCLUDE_DIRS}
    )
    # 链接 TRT + ONNX + CUDA 库
    target_link_libraries(3rd_tensorrt INTERFACE 
        ${TRT_INFER_LIB}
        ${TRT_ONNX_LIB}
        ${CUDA_LIBRARIES}
    )
    message(STATUS "TensorRT & CUDA paths integrated.")
else()
    add_library(3rd_tensorrt INTERFACE)
    message(WARNING "TensorRT NOT FOUND")
endif()