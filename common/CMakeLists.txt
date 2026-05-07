# common/CMakeLists.txt
# 通用工具模块

file(GLOB COMMON_SOURCES "src/*.cpp")
add_library(common_lib STATIC ${COMMON_SOURCES})

target_include_directories(common_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(common_lib
    PRIVATE
        Threads::Threads
)