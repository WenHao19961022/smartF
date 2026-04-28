# third_part/mqtt/CMakeLists.txt

# 查找已安装的Paho库（可选）
find_package(PahoMqttC QUIET)
find_package(PahoMqttCpp QUIET)

# 检查是否找到MQTT库
if(PahoMqttC_FOUND AND PahoMqttCpp_FOUND)
    message(STATUS "Found Paho MQTT libraries - MQTT support enabled")
    
    # 创建接口库供主项目使用
    add_library(3rd_mqtt INTERFACE)
    target_link_libraries(3rd_mqtt INTERFACE
        PahoMqttC::paho-mqtt3c
        PahoMqttCpp::paho-mqttpp3
    )
    set(WITH_MQTT ON CACHE BOOL "MQTT support" FORCE)
else()
    message(WARNING "Paho MQTT libraries not found - MQTT support disabled")
    
    # 创建空接口库，使链接不失败
    add_library(3rd_mqtt INTERFACE)
    set(WITH_MQTT OFF CACHE BOOL "MQTT support" FORCE)
endif()