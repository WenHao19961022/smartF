#include "../include/config_manager.h"
#include "../include/logger.h"
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
    // MQTT配置（云端服务器）
    mConfig["mqtt.broker_addr"] = "tcp://101.34.239.30:1883";
    mConfig["mqtt.client_id"] = "SmartFridge_MQTT_Client";
    mConfig["mqtt.username"] = "admin";
    mConfig["mqtt.password"] = "admin123";
    mConfig["mqtt.topic"] = "smartfridge/data";
    mConfig["mqtt.qos"] = "1";
    mConfig["mqtt.keepalive"] = "60";

    // 串口配置
    mConfig["serial.port"] = "/dev/ttyTHS0";
    mConfig["serial.baudrate"] = "115200";
    mConfig["serial.timeout_ms"] = "1000";
    mConfig["serial.weight_threshold"] = "100";
    mConfig["serial.temperature_threshold"] = "1";
    mConfig["serial.humidity_threshold"] = "5";

    // CV模型配置
    mConfig["cv.model_path"] = "cv_model/yolov8/yolov8s.engine";
    mConfig["cv.model_onnx_path"] = "cv_model/yolov8/yolov8s.onnx";
    mConfig["cv.conf_threshold"] = "0.25";
    mConfig["cv.nms_threshold"] = "0.4";
    mConfig["cv.min_fruit_box_area_ratio"] = "0.003";
    mConfig["cv.min_fruit_box_side"] = "36";
    mConfig["cv.location_calibration_enable"] = "0";
    mConfig["cv.location_x_min"] = "0.0";
    mConfig["cv.location_x_max"] = "1.0";
    mConfig["cv.location_y_min"] = "0.0";
    mConfig["cv.location_y_max"] = "1.0";
    mConfig["cv.location_flip_x"] = "0";
    mConfig["cv.location_flip_y"] = "0";
    mConfig["cv.rotten_spot_enable"] = "1";
    mConfig["cv.rotten_dark_ratio_threshold"] = "0.15";
    mConfig["cv.rotten_dark_ratio_threshold_orange"] = "0.18";
    mConfig["cv.rotten_dark_ratio_threshold_banana"] = "0.15";
    mConfig["cv.rotten_dark_v_threshold"] = "80";
    mConfig["cv.rotten_dark_s_threshold"] = "30";
    mConfig["cv.rotten_dark_v_threshold_orange"] = "130";
    mConfig["cv.rotten_dark_s_threshold_orange"] = "30";
    mConfig["cv.orange_color_reclass_enable"] = "1";
    mConfig["cv.orange_reclass_score_min"] = "0.03";
    mConfig["cv.orange_reclass_ratio_min"] = "0.45";
    mConfig["cv.orange_reclass_strong_ratio_min"] = "0.80";
    mConfig["cv.orange_to_apple_reclass_enable"] = "1";
    mConfig["cv.orange_to_apple_conf_max"] = "0.40";
    mConfig["cv.orange_to_apple_score_min"] = "0.03";
    mConfig["cv.orange_to_apple_dark_ratio_min"] = "0.30";
    mConfig["cv.orange_opencv_detect_enable"] = "1";
    mConfig["cv.orange_opencv_min_area_ratio"] = "0.008";
    mConfig["cv.orange_opencv_max_area_ratio"] = "0.18";
    mConfig["cv.orange_opencv_min_coverage"] = "0.55";
    mConfig["cv.orange_opencv_min_circularity"] = "0.35";
    mConfig["cv.orange_opencv_min_aspect"] = "0.55";
    mConfig["cv.orange_opencv_max_aspect"] = "1.35";
    mConfig["cv.bag_detect_enable"] = "1";
    mConfig["cv.bag_background_path"] = "";
    mConfig["cv.bag_red_s_min"] = "70";
    mConfig["cv.bag_red_v_min"] = "50";
    mConfig["cv.bag_red_min_area_ratio"] = "0.01";
    mConfig["cv.bag_red_reject_border_touch"] = "1";
    mConfig["cv.bag_red_border_margin_ratio"] = "0.03";
    mConfig["cv.bag_white_min_area_ratio"] = "0.02";
    mConfig["cv.bag_max_area_ratio"] = "0.45";
    mConfig["cv.bag_white_s_max"] = "90";
    mConfig["cv.bag_white_v_min"] = "105";
    mConfig["cv.bag_white_coverage_min"] = "0.18";
    mConfig["cv.bag_white_edge_ratio_min"] = "0.015";
    mConfig["cv.bag_white_inner_edge_ratio_min"] = "0.004";
    mConfig["cv.bag_white_stddev_min"] = "8";
    mConfig["cv.bag_white_border_contrast_min"] = "6";
    mConfig["cv.bag_white_glare_ratio_max"] = "0.55";
    mConfig["cv.bag_white_warm_fruit_ratio_max"] = "0.45";
    mConfig["cv.bag_ignore_top_ratio"] = "0.08";
    mConfig["cv.bag_white_reject_border_touch"] = "1";
    mConfig["cv.bag_white_border_margin_ratio"] = "0.03";
    mConfig["cv.bag_bg_diff_threshold"] = "40";
    mConfig["cv.bag_bg_min_area_ratio"] = "0.01";
    mConfig["cv.bag_bg_max_area_ratio"] = "0.45";
    mConfig["cv.bag_bg_border_margin_ratio"] = "0.03";
    mConfig["cv.bag_bg_white_coverage_min"] = "0.15";
    mConfig["cv.bag_bg_red_coverage_min"] = "0.04";
    mConfig["cv.bag_bg_edge_ratio_min"] = "0.012";
    mConfig["cv.bag_bg_close_kernel"] = "5";
    mConfig["cv.bag_white_low_contrast_enable"] = "1";
    mConfig["cv.bag_white_low_contrast_require_background"] = "1";
    mConfig["cv.bag_white_low_contrast_v_min"] = "180";
    mConfig["cv.bag_white_low_contrast_s_max"] = "90";
    mConfig["cv.bag_white_low_contrast_texture_threshold"] = "16";
    mConfig["cv.bag_white_low_contrast_top_ignore_ratio"] = "0.12";
    mConfig["cv.bag_white_low_contrast_bottom_limit_ratio"] = "0.55";
    mConfig["cv.bag_white_low_contrast_side_margin_ratio"] = "0.12";
    mConfig["cv.bag_white_low_contrast_min_area_ratio"] = "0.04";
    mConfig["cv.bag_white_low_contrast_max_area_ratio"] = "0.22";
    mConfig["cv.bag_white_low_contrast_min_width"] = "120";
    mConfig["cv.bag_white_low_contrast_min_height"] = "110";
    mConfig["cv.bag_white_low_contrast_max_bottom_ratio"] = "0.58";
    mConfig["cv.bag_white_low_contrast_center_x_min"] = "0.35";
    mConfig["cv.bag_white_low_contrast_center_x_max"] = "0.68";
    mConfig["cv.bag_white_low_contrast_min_aspect"] = "0.75";
    mConfig["cv.bag_white_low_contrast_max_aspect"] = "2.2";
    mConfig["cv.bag_white_low_contrast_min_coverage"] = "0.45";
    mConfig["cv.bag_white_low_contrast_mean_s_max"] = "55";
    mConfig["cv.bag_white_low_contrast_stddev_min"] = "18";
    mConfig["cv.bag_white_low_contrast_score_bias"] = "0.25";
    mConfig["cv.bag_fruit_overlap_max"] = "0.35";
    mConfig["cv.bag_merge_iou_threshold"] = "0.25";
    mConfig["cv.bag_merge_smaller_overlap"] = "0.55";
    mConfig["cv.bag_merge_max_gap_px"] = "20";
    mConfig["cv.bag_merge_min_axis_overlap"] = "0.45";
    mConfig["cv.bag_cross_source_merge_distance"] = "30";
    mConfig["cv.static_bag_confirm_threshold"] = "2";

    // 摄像头配置
    mConfig["camera.index"] = "0";
    mConfig["camera.width"] = "640";
    mConfig["camera.height"] = "480";
    mConfig["camera.fps"] = "30";

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
        LOG_PRINT("[Config]", "Failed to open config file: " << configPath);
        LOG_PRINT("[Config]", "Using default values.");
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        ParseLine(line);
    }

    LOG_PRINT("[Config]", "Loaded " << mConfig.size() << " settings from: " << configPath);
    return true;
}

bool ConfigManager::SaveConfig(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(mMutex);

    std::ofstream file(configPath);
    if (!file.is_open()) {
        LOG_PRINT("[Config]", "Failed to save config file: " << configPath);
        return false;
    }

    file << "# Smart Fridge Configuration File\n\n";

    for (const auto& [key, value] : mConfig) {
        file << key << " = " << value << "\n";
    }

    LOG_PRINT("[Config]", "Saved " << mConfig.size() << " settings to: " << configPath);
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
