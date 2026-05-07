#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <map>
#include <mutex>

class ConfigManager {
public:
    static ConfigManager& getInstance();

    // 加载配置文件
    bool loadConfig(const std::string& configPath = "config/smartfridge.conf");

    // 获取配置值
    std::string getString(const std::string& key, const std::string& defaultVal = "");
    int getInt(const std::string& key, int defaultVal = 0);
    float getFloat(const std::string& key, float defaultVal = 0.0f);
    bool getBool(const std::string& key, bool defaultVal = false);

    // 设置配置值（运行时）
    void setString(const std::string& key, const std::string& val);
    void setInt(const std::string& key, int val);
    void setFloat(const std::string& key, float val);
    void setBool(const std::string& key, bool val);

    // 保存配置到文件
    bool saveConfig(const std::string& configPath = "config/smartfridge.conf");

private:
    ConfigManager();
    ~ConfigManager();
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    std::map<std::string, std::string> config_;
    std::mutex mutex_;

    void trimString(std::string& str);
    void parseLine(const std::string& line);
};

#endif // CONFIG_MANAGER_H