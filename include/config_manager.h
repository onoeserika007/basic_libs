#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <json/json.h>
#include <string>
#include <mutex>
#include <optional>
#include <filesystem>
#include <variant>

constexpr const char* default_config_path = "conf/server.json";

class ConfigManager {
public:
    static ConfigManager& Instance();

    // 初始化配置管理器，加载配置文件
    bool init(const std::string& config_path = default_config_path);
    
    // 重置配置管理器状态
    void reset();

    // MySQL 配置项获取
    std::string getMySQLHost() const;
    std::string getMySQLUser() const;
    std::string getMySQLPassword() const;
    std::string getMySQLDatabase() const;
    unsigned int getMySQLPort() const;
    unsigned int getMySQLPoolSize() const;

    // HTTP服务器配置项获取
    unsigned int getServerPort() const;
    unsigned int getWorkerThreads() const;
    unsigned int getMaxConnections() const;
    std::string getDocumentRoot() const;
    unsigned int getTimeoutMs() const;

    // 日志配置项获取
    std::string getLogLevel() const;
    std::string getLogVisibleLevel() const;
    bool getLogAsync() const;
    std::string getLogPath() const;
    bool getLogToConsole() const;
    bool getLogToFile() const;
    unsigned int getLogFlushInterval() const;
    unsigned int getLogRollSize() const;

    // 获取任意配置项（高级用法）
    using ConfigValue = std::variant<std::string, int, unsigned int, bool, double>;
    ConfigValue get(const std::string& key) const;
    
    // 新增：将当前配置保存到文件
    bool saveConfig(const std::string& path) const;

private:
    ConfigManager();
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // 加载并验证配置
    bool loadConfig(const std::string& path);
    // 创建默认配置
    void createDefaultConfig(const std::string& path);
    // 验证配置完整性
    bool validateConfig();

    Json::Value config_;
    mutable std::mutex mutex_;
    bool initialized_{false};
    std::string config_path_{default_config_path}; // 默认配置文件路径
};

#endif // CONFIG_MANAGER_H