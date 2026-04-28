# 搜集主程序源文件
file(GLOB CORE_SOURCES "*.cpp")

# 生成可执行文件
add_executable(smart_fridge_app ${CORE_SOURCES})

# 链接子模块库
target_link_libraries(smart_fridge_app
    PRIVATE
    stm32_lib
    cv_model_lib
    mqtt_message_sender_lib
)