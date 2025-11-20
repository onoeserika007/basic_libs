#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <json/json.h>
#include <string>
#include <mutex>
#include <type_traits>
#include <iostream>
#include <sstream>
#include <vector>

constexpr const char* default_config_path = "conf/server.json";

class ConfigManager {
public:
    static ConfigManager& Instance();

    // 初始化配置管理器，加载配置文件
    bool init(const std::string& config_path = default_config_path);
    
    // 重置配置管理器状态
    void reset();

    // 模板化的 get 方法，用于获取任意类型的配置项
    template<typename T>
    T get(const std::string& key, const T& defaultValue) const {
        std::lock_guard<std::mutex> lock(mutex_);
        Json::Value val = resolvePath(config_, key);

        if (val.isNull()) {
            return defaultValue;
        }

        try {
            if constexpr (std::is_same<T, std::string>::value) {
                return val.asString();
            } else if constexpr (std::is_integral<T>::value && std::is_signed<T>::value) {
                return static_cast<T>(val.asInt64());
            } else if constexpr (std::is_integral<T>::value && std::is_unsigned<T>::value) {
                return static_cast<T>(val.asUInt64());
            } else if constexpr (std::is_floating_point<T>::value) {
                return static_cast<T>(val.asDouble());
            } else if constexpr (std::is_same<T, bool>::value) {
                return val.asBool();
            }
        } catch (const Json::Exception& e) {
            // 类型转换失败时返回默认值
            std::cerr << "ConfigManager::get type conversion error for key '" << key << "': " << e.what() << std::endl;
            return defaultValue;
        }
        
        return defaultValue;
    }

    // 模板化的 set 方法，用于设置配置项
    template<typename T>
    void set(const std::string& key, const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        Json::Value* target = &resolvePath(config_, key, true);
        *target = value;
    }
    
    // 将当前配置保存到文件
    bool saveConfig(const std::string& path) const;

    // 检查配置项是否存在
    bool has(const std::string& key) const;

private:
    ConfigManager();
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // 加载配置
    bool loadConfig(const std::string& path);

    // 辅助函数，用于解析点分隔的路径
    Json::Value& resolvePath(Json::Value& root, const std::string& path, bool create = false) const;
    const Json::Value& resolvePath(const Json::Value& root, const std::string& path) const;

    // 分割路径字符串
    std::vector<std::string> splitPath(const std::string& path) const;

    Json::Value config_;
    mutable std::mutex mutex_;
    bool initialized_{false};
    std::string config_path_{default_config_path}; // 默认配置文件路径
};

#endif // CONFIG_MANAGER_H