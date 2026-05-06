# core/CMakeLists.txt

# 搜集主程序源文件
file(GLOB CORE_SOURCES "src/*.cpp")

# 生成可执行文件
add_executable(smart_fridge_app ${CORE_SOURCES})

# 链接子模块库
target_link_libraries(smart_fridge_app
    PRIVATE
    cv_model_lib                # 高层业务库
    stm32_message_recever_lib
    mqtt_message_sender_lib
    3rd_tensorrt                # 显式链接 TensorRT + CUDA 接口库 (解决 undefined reference)
    3rd_opencv                  # 显式链接 OpenCV 接口库
    Threads::Threads
)