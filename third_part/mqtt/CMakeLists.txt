# third_part/mqtt/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 查找已安装的Paho库
find_package(PahoMqttC REQUIRED)
find_package(PahoMqttCpp REQUIRED)

message(STATUS "Using system Paho MQTT libraries")

# 您的MQTT相关代码
# ... 您的源代码配置 ...

# 链接系统安装的Paho库
target_link_libraries(your_target_name
    PahoMqttC::paho-mqtt3c
    PahoMqttCpp::paho-mqttpp3
    ${OpenSSL_LIBRARIES}
    pthread
)