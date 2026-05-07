#include "../include/config_manager.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cctype>

ConfigManager& ConfigManager::GetInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    // 设置默认值
    // MQTT配置
    mConfig["mqtt.broker_addr"] = "tcp://localhost:1883";
    mConfig["mqtt.client_id"] = "SmartFridge_MQTT_Client";
    mConfig["mqtt.topic"] = "smartfridge/data";
    mConfig["mqtt.qos"] = "1";
    mConfig["mqtt.keepalive"] = "60";

    // 串口配置
    mConfig["serial.port"] = "/dev/ttyUSB0";
    mConfig["serial.baudrate"] = "115200";
    mConfig["serial.timeout_ms"] = "1000";

    // CV模型配置
    mConfig["cv.model_path"] = "cv_model/yolov8/yolo12n.engine";
    mConfig["cv.conf_threshold"] = "0.5";
    mConfig["cv.nms_threshold"] = "0.4";
    mConfig["cv.use_simulation"] = "false";

    // 业务配置
    mConfig["device.id"] = "10001";
    mConfig["device.name"] = "SmartFridge-001";
    mConfig["inventory.static_interval_sec"] = "7200";  // 2小时
    mConfig["inventory.weight_threshold"] = "10";
    mConfig["inventory.max_fruit_count"] = "10";
}

ConfigManager::~ConfigManager() {
}

void ConfigManager::TrimString(std::string& str) {
    // 去除首尾空白
    str.erase(str.begin(),
              std::find_if(str.begin(), str.end(),
                          [](int ch) { return !std::isspace(ch); }));
    str.erase(std::find_if(str.rbegin(), str.rend(),
                          [](int ch) { return !std::isspace(ch); }).base(),
              str.end());
}

void ConfigManager::ParseLine(const std::string& line) {
    std::string trimmed = line;
    TrimString(trimmed);

    // 跳过空行和注释
    if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
        return;
    }

    // 查找等号
    size_t pos = trimmed.find('=');
    if (pos == std::string::npos) {
        return;
    }

    std::string key = trimmed.substr(0, pos);
    std::string value = trimmed.substr(pos + 1);

    TrimString(key);
    TrimString(value);

    // 去除引号
    if ((value.front() == '"' && value.back() == '"') ||
        (value.front() == '\'' && value.back() == '\'')) {
        value = value.substr(1, value.length() - 2);
    }

    mConfig[key] = value;
}

bool ConfigManager::LoadConfig(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(mMutex);

    std::ifstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "[Config] Failed to open config file: " << configPath << std::endl;
        std::cerr << "[Config] Using default values." << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        ParseLine(line);
    }

    std::cout << "[Config] Loaded " << mConfig.size() << " settings from: "
              << configPath << std::endl;
    return true;
}

bool ConfigManager::SaveConfig(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(mMutex);

    std::ofstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "[Config] Failed to save config file: " << configPath << std::endl;
        return false;
    }

    file << "# Smart Fridge Configuration File\n\n";

    for (const auto& [key, value] : mConfig) {
        file << key << " = " << value << "\n";
    }

    std::cout << "[Config] Saved " << mConfig.size() << " settings to: "
              << configPath << std::endl;
    return true;
}

std::string ConfigManager::GetString(const std::string& key,
                                     const std::string& defaultVal) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mConfig.find(key);
    return (it != mConfig.end()) ? it->second : defaultVal;
}

int ConfigManager::GetInt(const std::string& key, int defaultVal) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mConfig.find(key);
    if (it != mConfig.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultVal;
        }
    }
    return defaultVal;
}

float ConfigManager::GetFloat(const std::string& key, float defaultVal) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mConfig.find(key);
    if (it != mConfig.end()) {
        try {
            return std::stof(it->second);
        } catch (...) {
            return defaultVal;
        }
    }
    return defaultVal;
}

bool ConfigManager::GetBool(const std::string& key, bool defaultVal) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mConfig.find(key);
    if (it != mConfig.end()) {
        std::string val = it->second;
        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
        return (val == "true" || val == "1" || val == "yes" || val == "on");
    }
    return defaultVal;
}

void ConfigManager::SetString(const std::string& key, const std::string& val) {
    std::lock_guard<std::mutex> lock(mMutex);
    mConfig[key] = val;
}

void ConfigManager::SetInt(const std::string& key, int val) {
    std::lock_guard<std::mutex> lock(mMutex);
    mConfig[key] = std::to_string(val);
}

void ConfigManager::SetFloat(const std::string& key, float val) {
    std::lock_guard<std::mutex> lock(mMutex);
    mConfig[key] = std::to_string(val);
}

void ConfigManager::SetBool(const std::string& key, bool val) {
    std::lock_guard<std::mutex> lock(mMutex);
    mConfig[key] = val ? "true" : "false";
}