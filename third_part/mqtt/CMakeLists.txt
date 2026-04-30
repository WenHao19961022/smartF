# third_part/mqtt/CMakeLists.txt

message(STATUS "Using manual discovery for Paho MQTT to avoid ALIAS visibility issues")

# 直接寻找库文件和头文件
find_library(MQTT_C_LIB NAMES paho-mqtt3as PATHS /usr/local/lib NO_DEFAULT_PATH)
find_library(MQTT_C_LIB NAMES paho-mqtt3as) # 备选搜索

find_library(MQTT_CPP_LIB NAMES paho-mqttpp3 PATHS /usr/local/lib NO_DEFAULT_PATH)
find_library(MQTT_CPP_LIB NAMES paho-mqttpp3) # 备选搜索

find_path(MQTT_C_INC NAMES MQTTAsync.h PATHS /usr/local/include NO_DEFAULT_PATH)
find_path(MQTT_CPP_INC NAMES mqtt/async_client.h PATHS /usr/local/include NO_DEFAULT_PATH)

if(MQTT_C_LIB AND MQTT_CPP_LIB)
    add_library(3rd_mqtt INTERFACE)
    # 链接具体的库文件路径，避开不稳定的 ALIAS 目标
    target_link_libraries(3rd_mqtt INTERFACE ${MQTT_CPP_LIB} ${MQTT_C_LIB})
    target_include_directories(3rd_mqtt INTERFACE ${MQTT_C_INC} ${MQTT_CPP_INC})
    
    message(STATUS "Found MQTT C Lib: ${MQTT_C_LIB}")
    message(STATUS "Found MQTT C++ Lib: ${MQTT_CPP_LIB}")
    set(WITH_MQTT ON CACHE BOOL "MQTT support" FORCE)
else()
    message(FATAL_ERROR "MQTT Libraries NOT found in /usr/local. Please verify the installation.")
endif()