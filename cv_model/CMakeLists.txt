# 1. 寻找第三方依赖 (OpenCV 是视觉处理的基础)
find_package(OpenCV REQUIRED)

# 2. 搜集源文件（包括 YOLO12/RT-DETR 的推理实现、TensorRT 封装等）
# 建议将具体实现放在 src 目录下
file(GLOB CV_MODEL_SOURCES CONFIGURE_DEPENDS "src/*.cpp" "*.cpp")

# 3. 生成静态库 cv_model_lib
add_library(cv_model_lib STATIC ${CV_MODEL_SOURCES})

# 4. 精细化控制头文件可见性
target_include_directories(cv_model_lib 
    # PUBLIC: 只有这里的 API（如 detect_fruit()）能被 core 主进程调用
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/api

    # PRIVATE: 包含 YOLO 模型加载逻辑、图像预处理等私有头文件
    # 同时包含 OpenCV 的头文件（如果 core 不需要直接调用 OpenCV 的话）
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
)

# 5. 链接第三方库
# 这里将 OpenCV 设为 PRIVATE，意味着 core 只需要调用你的 API，
# 而不需要关心底层是用 OpenCV 还是其他库实现的。
target_link_libraries(cv_model_lib 
    PRIVATE 
        ${OpenCV_LIBS}
        # 可选 TensorRT 支持
        $<$<TARGET_EXISTS:3rd_tensorrt>:3rd_tensorrt>
)