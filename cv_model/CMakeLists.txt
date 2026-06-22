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

if(BUILD_TESTING)
    add_executable(static_scene_replay
        tests/static_scene_replay.cpp
        src/static_scene_analyzer.cpp
    )
    target_include_directories(static_scene_replay PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/api
        ${CMAKE_SOURCE_DIR}/common/include
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(static_scene_replay PRIVATE ${OpenCV_LIBS} common_lib)
    add_test(NAME static_scene_empty_reference
        COMMAND static_scene_replay
            ${CMAKE_SOURCE_DIR}/config/fridge_empty_bg.jpg
            --empty
            ${CMAKE_SOURCE_DIR}/config/fridge_empty_bg.jpg)
endif()
