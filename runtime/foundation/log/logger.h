// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace common::log {

enum class LogLevel {
    kDebug,
    kInfo,
    kWarn,
    kError,
};

enum class LogFormat {
    kAuto,
    kText,
    kJson,
};

class Logger {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    struct LogRecord {
        std::chrono::system_clock::time_point timestamp;
        LogLevel level = LogLevel::kInfo;
        std::string service_name = "service";
        std::string service_instance_id;
        std::string environment;
        LogFormat format = LogFormat::kText;
        std::string message;
    };

    static Logger& Instance();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void SetServiceName(std::string service_name);
    void SetServiceInstanceId(std::string service_instance_id);
    void SetEnvironment(std::string environment);
    void SetMinLogLevel(LogLevel level);
    bool SetMinLogLevel(std::string_view level_name);
    void SetLogFormat(LogFormat format);
    bool SetLogFormat(std::string_view format_name);
    void Log(LogLevel level, std::string_view message);
    void LogSync(LogLevel level, std::string_view message);
    void Flush();
    void Shutdown();

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    Logger();
    ~Logger();

    [[nodiscard]] std::string LevelToString(LogLevel level) const;
    [[nodiscard]] LogRecord BuildRecord(LogLevel level, std::string_view message) const;
    [[nodiscard]] LogLevel CurrentMinLogLevel() const;
    [[nodiscard]] bool ShouldLog(LogLevel level) const;
    [[nodiscard]] LogFormat ResolveLogFormat(std::string_view environment) const;
    void WriteRecord(LogRecord record);

    struct Impl;
    std::unique_ptr<Impl> impl_;

    mutable std::mutex metadata_mutex_;
    std::string service_name_ = "service";
    std::string service_instance_id_;
    std::string environment_;
    LogLevel min_log_level_ = LogLevel::kInfo;
    LogFormat log_format_ = LogFormat::kAuto;
    mutable std::mutex output_mutex_;
};

}  // namespace common::log
