# third_part/mqtt/CMakeLists.txt

# 寻找系统安装的 Paho MQTT 库
find_package(PahoMqttCpp QUIET)

if(PahoMqttCpp_FOUND)
    # 创建接口库供主项目使用
    add_library(3rd_mqtt INTERFACE)
    target_link_libraries(3rd_mqtt INTERFACE PahoMqttCpp::paho-mqttpp3-static)
    target_include_directories(3rd_mqtt INTERFACE ${PahoMqttCpp_INCLUDE_DIRS})
    message(STATUS "Found PahoMqttCpp")
else()
    # 如果找不到，创建空的接口库（MQTT功能将不可用）
    add_library(3rd_mqtt INTERFACE)
    message(WARNING "PahoMqttCpp not found! MQTT functionality will be disabled.")
endif()