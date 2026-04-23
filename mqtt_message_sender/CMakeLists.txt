# 1. 搜集源文件（包括私有实现）
file(GLOB SERVER_SOURCES "src/*.cpp" "*.cpp")

# 2. 生成静态库
add_library(server_lib STATIC ${SERVER_SOURCES})

# 3. 精细化控制头文件可见性
target_include_directories(server_lib 
    # PUBLIC: 对外公开。core 模块可以 #include 这里面的头文件。
    # 建议将对外暴露的 API（如 stm32_api.h）放在这个文件夹下。
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/api 

    # PRIVATE: 内部私有。存放具体的实现细节（如底层的 GPIO 配置、寄存器操作头文件）。
    # 这些文件只在编译 stm32_lib 时有用，core 模块无法引用它们，保证了安全性。
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)