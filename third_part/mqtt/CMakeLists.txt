# third_part/mqtt/CMakeLists.txt

# --- 1. Paho C 配置 ---
set(PAHO_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(PAHO_WITH_SSL TRUE CACHE BOOL "" FORCE)
set(PAHO_BUILD_STATIC ON CACHE BOOL "" FORCE)

add_subdirectory(paho.mqtt.c)

# --- 2. Paho C++ 配置 ---
set(PAHO_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)

# 关键：告诉 C++ 库，C 库的源码路径在哪里（用于找头文件）
set(PAHO_MQTT_C_PATH ${CMAKE_CURRENT_SOURCE_DIR}/paho.mqtt.c CACHE PATH "" FORCE)
set(PAHO_MQTT_C_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/paho.mqtt.c/src CACHE PATH "" FORCE)

# 关键：手动链接 C 库的目标名（如果你在 C 库里生成了静态库）
set(PAHO_MQTT_C_LIBRARIES paho-mqtt3as-static CACHE STRING "" FORCE)

add_subdirectory(paho.mqtt.cpp)