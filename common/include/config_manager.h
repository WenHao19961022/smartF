#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <map>
#include <mutex>

// ==================== 配置管理类 ====================
class ConfigManager {
public:
    static ConfigManager& GetInstance();

    bool LoadConfig(const std::string& configPath = "config/smartfridge.conf");

    std::string GetString(const std::string& key, const std::string& defaultVal = "");
    int GetInt(const std::string& key, int defaultVal = 0);
    float GetFloat(const std::string& key, float defaultVal = 0.0f);
    bool GetBool(const std::string& key, bool defaultVal = false);

    void SetString(const std::string& key, const std::string& val);
    void SetInt(const std::string& key, int val);
    void SetFloat(const std::string& key, float val);
    void SetBool(const std::string& key, bool val);

    bool SaveConfig(const std::string& configPath = "config/smartfridge.conf");

private:
    ConfigManager();
    ~ConfigManager();

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    void TrimString(std::string& str);
    void ParseLine(const std::string& line);

    std::map<std::string, std::string> mConfig;
    std::mutex mMutex;
};

#endif // CONFIG_MANAGER_H