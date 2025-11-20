#include "serika/basic/config_manager.h"
#include <fstream>
#include <sstream>
#include <vector>
#include "serika/basic/logger.h"

ConfigManager & ConfigManager::Instance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    // 构造函数中尝试加载默认配置文件
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        // 尝试加载默认配置文件
        if (loadConfig(config_path_)) {
            initialized_ = true;
        } else {
            // 如果默认配置文件不存在，创建空配置对象
            config_ = Json::Value(Json::objectValue);
            initialized_ = true;
        }
    }
}

bool ConfigManager::init(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        std::cout << "ConfigManager already initialized" << std::endl;
        reset();
    }

    config_path_ = config_path;
    
    if (!loadConfig(config_path)) {
        std::cerr << "Failed to load config file: " << config_path << ", using empty configuration" << std::endl;
        config_ = Json::Value(Json::objectValue);
    }

    initialized_ = true;
    return true;
}

void ConfigManager::reset() {
    config_ = Json::Value();
    initialized_ = false;
    config_path_ = default_config_path;
}

bool ConfigManager::loadConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Config file not found: " << path << std::endl;
        return false;
    }

    Json::CharReaderBuilder builder;
    Json::CharReader* reader = builder.newCharReader();
    std::string errors;

    std::string content((std::istreambuf_iterator<char>(file)), 
                        std::istreambuf_iterator<char>());
    bool success = reader->parse(content.c_str(), content.c_str() + content.size(), 
                               &config_, &errors);
    delete reader;

    if (!success) {
        std::cerr << "Failed to parse config file: " << errors << std::endl;
        return false;
    }

    return true;
}

bool ConfigManager::saveConfig(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Json::StreamWriterBuilder writer;
    writer["commentStyle"] = "None";
    writer["indentation"] = "    ";
    std::unique_ptr<Json::StreamWriter> streamWriter(writer.newStreamWriter());

    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << path << std::endl;
        return false;
    }

    streamWriter->write(config_, &file);
    file.close();
    return true;
}

bool ConfigManager::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const Json::Value& val = resolvePath(config_, key);
    return !val.isNull();
}

std::vector<std::string> ConfigManager::splitPath(const std::string& path) const {
    std::vector<std::string> parts;
    std::string current;
    
    for (char c : path) {
        if (c == '.') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    
    if (!current.empty()) {
        parts.push_back(current);
    }
    
    return parts;
}

Json::Value& ConfigManager::resolvePath(Json::Value& root, const std::string& path, bool create) const {
    if (path.empty()) {
        return root;
    }
    
    auto parts = splitPath(path);
    Json::Value* current = &root;
    
    for (const auto& part : parts) {
        if (current->isNull()) {
            if (create) {
                *current = Json::Value(Json::objectValue);
            } else {
                static Json::Value nullValue;
                return nullValue;
            }
        }
        
        if (!current->isObject()) {
            if (create) {
                *current = Json::Value(Json::objectValue);
            } else {
                static Json::Value nullValue;
                return nullValue;
            }
        }
        
        if (create && !current->isMember(part)) {
            (*current)[part] = Json::Value();
        }
        
        current = &(*current)[part];
    }
    
    return *current;
}

const Json::Value& ConfigManager::resolvePath(const Json::Value& root, const std::string& path) const {
    if (path.empty()) {
        return root;
    }
    
    auto parts = splitPath(path);
    const Json::Value* current = &root;
    
    for (const auto& part : parts) {
        if (current->isNull() || !current->isObject() || !current->isMember(part)) {
            static Json::Value nullValue;
            return nullValue;
        }
        current = &(*current)[part];
    }
    
    return *current;
}