# 搜集主程序源文件
file(GLOB CORE_SOURCES "*.cpp")

# 生成可执行文件
add_executable(smart_fridge_app ${CORE_SOURCES})

# 链接子模块库
# 这样 core 就能自动获得 stm32 和 cv_model 的函数以及头文件路径
target_link_libraries(smart_fridge_app 
    PRIVATE 
    stm32_lib 
    cv_model_lib   # 假设 cv_model 目录下的库名叫这个
    ${OpenCV_LIBS}
)