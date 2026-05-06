# third_part/tensorrt/CMakeLists.txt 建议检查/修改为：

# 定义 CUDA 路径 (Jetson 默认)
set(CUDA_PATH "/usr/local/cuda")

# 寻找 TensorRT 库 (注意：一定要找到 libnvinfer.so)
find_library(TRT_INFER_LIB nvinfer PATHS /usr/lib/aarch64-linux-gnu /usr/local/cuda/lib64)
find_library(CUDA_RUNTIME_LIB cudart PATHS ${CUDA_PATH}/lib64)

if(TRT_INFER_LIB AND CUDA_RUNTIME_LIB)
    add_library(3rd_tensorrt INTERFACE)
    target_include_directories(3rd_tensorrt INTERFACE 
        /usr/include/aarch64-linux-gnu 
        ${CUDA_PATH}/include
    )
    # 这一步是解决 "undefined reference" 的关键
    target_link_libraries(3rd_tensorrt INTERFACE 
        ${TRT_INFER_LIB} 
        ${CUDA_RUNTIME_LIB}
    )
    message(STATUS "Found TensorRT: ${TRT_INFER_LIB}")
endif()