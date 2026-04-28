# third_part/CMakeLists.txt

# 改为使用find_package查找已安装的库
find_package(PahoMqttC REQUIRED)
find_package(PahoMqttCpp REQUIRED)

message(STATUS "Found Paho MQTT C: ${PahoMqttC_VERSION}")
message(STATUS "Found Paho MQTT C++: ${PahoMqttCpp_VERSION}")

# 如果find_package找不到，可以手动设置路径
# set(PAHO_MQTT_C_INCLUDE_DIRS /usr/local/include)
# set(PAHO_MQTT_C_LIBRARIES /usr/local/lib/libpaho-mqtt3c.so)
# set(PAHO_MQTT_CPP_INCLUDE_DIRS /usr/local/include)
# set(PAHO_MQTT_CPP_LIBRARIES /usr/local/lib/libpaho-mqttpp3.so)

# 添加其他模块
add_subdirectory(mqtt)  # 您的自定义MQTT模块
add_subdirectory(opencv)
add_subdirectory(tensorrt)  # 暂时禁用