# core/CMakeLists.txt

# 联调测试开关：启用 Mock STM32（无真实硬件时使用）
# 编译时加上 -DUSE_MOCK_STM32=1 或设为 true 来启用
# add_compile_definitions(USE_MOCK_STM32=1)

# 搜集主程序源文件
file(GLOB CORE_SOURCES "src/*.cpp")

# 生成可执行文件
add_executable(smart_fridge_app ${CORE_SOURCES})

# 链接子模块库
target_link_libraries(smart_fridge_app
    PRIVATE
        common_lib                  # 通用配置管理
        cv_model_lib                # 业务逻辑库
        stm32_message_recever_lib
        mqtt_message_sender_lib
        3rd_tensorrt                # 重要：提供 TensorRT 和 CUDA 的符号实现
        3rd_opencv                  # 提供 OpenCV 的符号实现
        Threads::Threads
)