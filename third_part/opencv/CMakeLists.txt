# third_part/opencv/CMakeLists.txt

# 寻找系统安装的 OpenCV
find_package(OpenCV REQUIRED)

if(OpenCV_FOUND)
    message(STATUS "Found OpenCV version: ${OpenCV_VERSION}")
    # 创建一个接口库，方便主工程统一调用
    add_library(3rd_opencv INTERFACE)
    target_include_directories(3rd_opencv INTERFACE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(3rd_opencv INTERFACE ${OpenCV_LIBS})
endif()