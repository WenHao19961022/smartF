#include "config_manager.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cctype>

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    // 设置默认值
    // MQTT配置
    config_["mqtt.broker_addr"] = "tcp://localhost:1883";
    config_["mqtt.client_id"] = "SmartFridge_MQTT_Client";
    config_["mqtt.topic"] = "smartfridge/data";
    config_["mqtt.qos"] = "1";
    config_["mqtt.keepalive"] = "60";

    // 串口配置
    config_["serial.port"] = "/dev/ttyUSB0";
    config_["serial.baudrate"] = "115200";
    config_["serial.timeout_ms"] = "1000";

    // CV模型配置
    config_["cv.model_path"] = "cv_model/yolov8/yolo12n.engine";
    config_["cv.conf_threshold"] = "0.5";
    config_["cv.nms_threshold"] = "0.4";
    config_["cv.use_simulation"] = "false";

    // 业务配置
    config_["device.id"] = "10001";
    config_["device.name"] = "SmartFridge-001";
    config_["inventory.static_interval_sec"] = "7200";  // 2小时
    config_["inventory.weight_threshold"] = "10";
    config_["inventory.max_fruit_count"] = "10";
}

ConfigManager::~ConfigManager() {
}

void ConfigManager::trimString(std::string& str) {
    // 去除首尾空白
    str.erase(str.begin(),
              std::find_if(str.begin(), str.end(),
                          [](int ch) { return !std::isspace(ch); }));
    str.erase(std::find_if(str.rbegin(), str.rend(),
                          [](int ch) { return !std::isspace(ch); }).base(),
              str.end());
}

void ConfigManager::parseLine(const std::string& line) {
    std::string trimmed = line;
    trimString(trimmed);

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

    trimString(key);
    trimString(value);

    // 去除引号
    if ((value.front() == '"' && value.back() == '"') ||
        (value.front() == '\'' && value.back() == '\'')) {
        value = value.substr(1, value.length() - 2);
    }

    config_[key] = value;
}

bool ConfigManager::loadConfig(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "[Config] Failed to open config file: " << configPath << std::endl;
        std::cerr << "[Config] Using default values." << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        parseLine(line);
    }

    std::cout << "[Config] Loaded " << config_.size() << " settings from: "
              << configPath << std::endl;
    return true;
}

bool ConfigManager::saveConfig(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ofstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "[Config] Failed to save config file: " << configPath << std::endl;
        return false;
    }

    file << "# Smart Fridge Configuration File\n\n";

    for (const auto& [key, value] : config_) {
        file << key << " = " << value << "\n";
    }

    std::cout << "[Config] Saved " << config_.size() << " settings to: "
              << configPath << std::endl;
    return true;
}

std::string ConfigManager::getString(const std::string& key,
                                     const std::string& defaultVal) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = config_.find(key);
    return (it != config_.end()) ? it->second : defaultVal;
}

int ConfigManager::getInt(const std::string& key, int defaultVal) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = config_.find(key);
    if (it != config_.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultVal;
        }
    }
    return defaultVal;
}

float ConfigManager::getFloat(const std::string& key, float defaultVal) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = config_.find(key);
    if (it != config_.end()) {
        try {
            return std::stof(it->second);
        } catch (...) {
            return defaultVal;
        }
    }
    return defaultVal;
}

bool ConfigManager::getBool(const std::string& key, bool defaultVal) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = config_.find(key);
    if (it != config_.end()) {
        std::string val = it->second;
        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
        return (val == "true" || val == "1" || val == "yes" || val == "on");
    }
    return defaultVal;
}

void ConfigManager::setString(const std::string& key, const std::string& val) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_[key] = val;
}

void ConfigManager::setInt(const std::string& key, int val) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_[key] = std::to_string(val);
}

void ConfigManager::setFloat(const std::string& key, float val) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_[key] = std::to_string(val);
}

void ConfigManager::setBool(const std::string& key, bool val) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_[key] = val ? "true" : "false";
}