# 1. 搜集源文件（包括私有实现）
file(GLOB SERVER_SOURCES "src/*.cpp" "*.cpp")

# 2. 生成静态库
add_library(mqtt_message_sender_lib STATIC ${SERVER_SOURCES})

# 3. 精细化控制头文件可见性
target_include_directories(mqtt_message_sender_lib
    # PUBLIC: 对外公开。core 模块可以 #include 这里面的头文件。
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/api
        ${CMAKE_SOURCE_DIR}/cv_model/api
        ${CMAKE_SOURCE_DIR}/stm32_message_recever/api

    # PRIVATE: 内部私有。
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 4. 链接第三方库
target_link_libraries(mqtt_message_sender_lib PRIVATE 3rd_mqtt)