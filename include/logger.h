//
// Created by inory on 10/28/25.
//

#ifndef LOGGER_H
#define LOGGER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <format>
#include <sys/time.h>

// 简易自旋锁（基于C++20 std::atomic_flag，轻量无锁竞争）
class SpinLock {
public:
    void lock() {
        // 自旋等待，直到获取锁（test_and_set返回false表示成功）
        while (m_flag.test_and_set(std::memory_order_acquire))
            ;
    }
    void unlock() {
        // 释放锁（clear操作，内存序release）
        m_flag.clear(std::memory_order_release);
    }

private:
    std::atomic_flag m_flag = ATOMIC_FLAG_INIT; // C++20 初始化宏
};

enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

class Logger {
public:
    // 获取单例
    static Logger &Instance();

    // 初始化：file_name (base name), async: 是否异步, max_queue_size: 异步队列长度(0 表示同步),
    // buf_size: 单条消息最大缓冲, rotate_bytes: 文件大小上限（0表示不按大小分割），close_log: 是否关闭日志输出（0:
    // 启用）
    bool Init(std::string file_name, bool async = true, size_t max_queue_size = 10000, size_t buf_size = 8192,
              size_t rotate_bytes = 50 * 1024 * 1024, int close_log = 0);

    // 写日志（与经典API兼容，format采用printf样式）
    template<typename... Args>
    void Log(LogLevel level, const char *fmt, Args &&...args);

    // 手动flush并关闭（会等待后台写线程完成）
    void Shutdown();

    // 是否关闭日志（外部可读）
    bool IsClosed() const { return m_close_log_; }

    std::string GetCurrentLogName() const { return m_current_name_; }

    // 禁止拷贝
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

private:
    Logger();
    ~Logger();

    // 后台线程主函数
    void WorkerThread();

    // 将格式化的日志写入文件（只由后台线程或同步路径调用，外部应加锁或保证单线程）
    void WriteToFile(const std::string &msg);

    // 检查是否需要切分文件（按天或按大小）
    void RotateIfNeeded();

    // 格式化时间前缀
    std::string MakeTimePrefix(struct timeval tv, LogLevel level);

    // 生成新的日志文件名
    std::string GenerateLogFileName();

private:
    // 配置
    std::string m_base_name_;
    size_t m_buf_size_;
    size_t m_rotate_bytes_;
    int m_close_log_;
    bool m_realtime_ = true;
    LogLevel m_visible_log_level_; // 日志可见级别

    // runtime
    std::string m_current_name_;

    // 文件句柄与相关计数
    std::unique_ptr<std::ofstream> m_file_stream_;
    std::ostream *m_output_stream_;
    size_t m_written_bytes_; // 当前文件已写字节数
    int m_today_; // 当前日期（tm_mday）

    // 异步队列与线程
    std::deque<std::string> m_queue_;
    SpinLock m_queue_lock_; // 轻量自旋锁，比std::mutex开销小 真的吗
    size_t m_max_queue_size_;
    std::mutex m_queue_mutex_;
    std::thread m_worker_;
    std::atomic<bool> m_running_;
    bool m_is_async_;

    // batched flush
    std::mutex m_file_mutex_;
    std::string m_batch_buf_; // 批量写入缓冲区
    size_t m_batch_flush_threshold_; // 缓冲区阈值（超阈值则刷盘）
};

template<typename... Args>
void Logger::Log(LogLevel level, const char *fmt, Args &&...args) {
    if (m_close_log_)
        return;

    if (level < m_visible_log_level_) {
        return;
    }

    // 2.1 可变参数格式化（C++20 std::vformat）
    std::string formatted_msg = std::vformat(fmt, std::make_format_args(args...));

    // 拼接最终日志（时间前缀+格式化内容+换行）
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    std::string time_prefix = MakeTimePrefix(tv, level);
    std::string final_msg = std::format("{}{}\n", time_prefix, formatted_msg);

    // 异步模式：入队（自旋锁保护，轻量）
    if (m_is_async_) {
        // 队列满时自旋等待（匹配原逻辑）
        while (true) {
            {
                std::lock_guard<SpinLock> lk(m_queue_lock_);
                if (m_queue_.size() < m_max_queue_size_) {
                    m_queue_.emplace_back(std::move(final_msg));
                    break;
                }
            }
            if (!m_running_.load())
                return;
            std::this_thread::yield(); // 避免CPU空转
        }
    } else {
        // 同步模式：直接写入
        WriteToFile(final_msg);
    }
}

//
// 方便宏（内部会检查是否关闭日志）
//
// 不用花括号是因为防止;打断if else逻辑
#define LOG_DEBUG(format, ...)                                                                                         \
    do {                                                                                                               \
        if (!Logger::Instance().IsClosed())                                                                            \
            Logger::Instance().Log(LogLevel::DEBUG, format, ##__VA_ARGS__);                                            \
    } while (0)
#define LOG_INFO(format, ...)                                                                                          \
    do {                                                                                                               \
        if (!Logger::Instance().IsClosed())                                                                            \
            Logger::Instance().Log(LogLevel::INFO, format, ##__VA_ARGS__);                                             \
    } while (0)
#define LOG_WARN(format, ...)                                                                                          \
    do {                                                                                                               \
        if (!Logger::Instance().IsClosed())                                                                            \
            Logger::Instance().Log(LogLevel::WARN, format, ##__VA_ARGS__);                                             \
    } while (0)
#define LOG_ERROR(format, ...)                                                                                         \
    do {                                                                                                               \
        if (!Logger::Instance().IsClosed())                                                                            \
            Logger::Instance().Log(LogLevel::ERROR, format, ##__VA_ARGS__);                                            \
    } while (0)

#endif // LOGGER_H