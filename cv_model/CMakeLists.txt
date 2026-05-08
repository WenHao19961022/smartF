# --- cv_model/CMakeLists.txt ---

file(GLOB CV_MODEL_SOURCES CONFIGURE_DEPENDS "src/*.cpp" "*.cpp")
add_library(cv_model_lib STATIC ${CV_MODEL_SOURCES})

target_include_directories(cv_model_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/api
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/common/include
        ${OpenCV_INCLUDE_DIRS}
        "/usr/local/cuda/include"  # 强制补入 CUDA 头文件路径
)

target_link_libraries(cv_model_lib
    PRIVATE
        ${OpenCV_LIBS}
        3rd_tensorrt  # 链接上面定义好的接口库
        common_lib
)