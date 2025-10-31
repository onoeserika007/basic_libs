#include "config_manager.h"
#include "logger.h"
#include <fstream>

ConfigManager& ConfigManager::Instance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    // 构造函数中尝试加载默认配置文件
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        // 尝试加载默认配置文件
        if (loadConfig(config_path_)) {
            validateConfig();
            initialized_ = true;
        }
    }
}

bool ConfigManager::init(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        LOG_WARN("ConfigManager already initialized");
        // 如果已经初始化，先重置状态
        reset();
    }

    config_path_ = config_path;
    
    if (!loadConfig(config_path)) {
        LOG_ERROR("Failed to load config file: {:s}", config_path);
        return false;
    }

    if (!validateConfig()) {
        LOG_ERROR("Config validation failed");
        return false;
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
        LOG_INFO("Config file not found, creating default config at: {:s}", path);
        createDefaultConfig(path);
        file.open(path);
        if (!file.is_open()) {
            LOG_ERROR("Failed to create default config file: {:s}", path);
            return false;
        }
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
        LOG_ERROR("Failed to parse config file: {:s}", errors);
        return false;
    }

    return true;
}

void ConfigManager::createDefaultConfig(const std::string& path) {
    Json::Value root;

    // MySQL配置
    root["mysql"]["host"] = "localhost";
    root["mysql"]["port"] = 3306;
    root["mysql"]["user"] = "webserver";
    root["mysql"]["password"] = "password";
    root["mysql"]["database"] = "webserver";
    root["mysql"]["pool_size"] = 8;
    root["mysql"]["connect_timeout"] = 10;
    root["mysql"]["read_timeout"] = 30;
    root["mysql"]["write_timeout"] = 30;

    // HTTP服务器配置
    root["server"]["port"] = 8080;
    root["server"]["worker_threads"] = 4;
    root["server"]["max_connections"] = 10000;
    root["server"]["document_root"] = "root";
    root["server"]["timeout_ms"] = 10000;

    // 日志配置
    root["log"]["level"] = "info";
    root["log"]["visible_level"] = "info";
    root["log"]["async"] = true;
    root["log"]["path"] = "logs";
    root["log"]["to_console"] = true;
    root["log"]["to_file"] = true;
    root["log"]["flush_interval"] = 3; // seconds
    root["log"]["roll_size"] = 5000000;

    Json::StreamWriterBuilder writer;
    writer["commentStyle"] = "None";
    writer["indentation"] = "    ";
    std::unique_ptr<Json::StreamWriter> streamWriter(writer.newStreamWriter());

    std::ofstream file(path);
    if (file.is_open()) {
        streamWriter->write(root, &file);
        file.close();
    }
}

bool ConfigManager::validateConfig() {
    // 检查必需的配置项
    const char* requiredFields[] = {
        "mysql.host", "mysql.port", "mysql.user", "mysql.password", "mysql.database",
        "mysql.pool_size", "server.port", "server.worker_threads", 
        "server.max_connections", "server.document_root", "log.path",
        "log.to_console", "log.to_file", "log.flush_interval"
    };

    for (const auto& field : requiredFields) {
        Json::Value val = Json::Path(field).resolve(config_);
        if (val.isNull()) {
            // 此时log有可能未初始化完成
            std::cerr << "Missing required config field: " << field << std::endl;
            return false;
        }
    }

    return true;
}

// MySQL 配置项获取
std::string ConfigManager::getMySQLHost() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["mysql"]["host"].asString();
}

std::string ConfigManager::getMySQLUser() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["mysql"]["user"].asString();
}

std::string ConfigManager::getMySQLPassword() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["mysql"]["password"].asString();
}

std::string ConfigManager::getMySQLDatabase() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["mysql"]["database"].asString();
}

unsigned int ConfigManager::getMySQLPort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["mysql"]["port"].asUInt();
}

unsigned int ConfigManager::getMySQLPoolSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["mysql"]["pool_size"].asUInt();
}

// HTTP服务器配置项获取
unsigned int ConfigManager::getServerPort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["server"]["port"].asUInt();
}

unsigned int ConfigManager::getWorkerThreads() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["server"]["worker_threads"].asUInt();
}

unsigned int ConfigManager::getMaxConnections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["server"]["max_connections"].asUInt();
}

std::string ConfigManager::getDocumentRoot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["server"]["document_root"].asString();
}

unsigned int ConfigManager::getTimeoutMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["server"]["timeout_ms"].asUInt();
}

// 日志配置项获取
std::string ConfigManager::getLogLevel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["log"]["level"].asString();
}

std::string ConfigManager::getLogVisibleLevel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["log"]["visible_level"].asString();
}

bool ConfigManager::getLogAsync() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["log"]["async"].asBool();
}

std::string ConfigManager::getLogPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["log"]["path"].asString();
}

bool ConfigManager::getLogToConsole() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["log"]["to_console"].asBool();
}

bool ConfigManager::getLogToFile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["log"]["to_file"].asBool();
}

unsigned int ConfigManager::getLogFlushInterval() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["log"]["flush_interval"].asUInt();
}

unsigned int ConfigManager::getLogRollSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_["log"]["roll_size"].asUInt();
}

ConfigManager::ConfigValue ConfigManager::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value val = Json::Path(key).resolve(config_);
    if (val.isNull()) {
        return std::string();
    }

    switch (val.type()) {
        case Json::stringValue:
            return val.asString();
        case Json::intValue:
            return val.asInt();
        case Json::uintValue:
            return val.asUInt();
        case Json::booleanValue:
            return val.asBool();
        case Json::realValue:
            return val.asDouble();
        default:
            return std::string();
    }
}

bool ConfigManager::saveConfig(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Json::StreamWriterBuilder writer;
    writer["commentStyle"] = "None";
    writer["indentation"] = "    ";
    std::unique_ptr<Json::StreamWriter> streamWriter(writer.newStreamWriter());

    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for writing: {:s}", path);
        return false;
    }

    streamWriter->write(config_, &file);
    file.close();
    return true;
}