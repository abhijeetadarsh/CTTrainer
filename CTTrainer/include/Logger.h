#pragma once
// ============================================================
//  Logger.h  --  Debug logger, writes to file + in-app console
// ============================================================
#include <string>
#include <vector>
#include <mutex>

enum class LogLevel { INFO, WARN, ERR, DEBUG };

struct LogEntry
{
    LogLevel    level;
    std::string timestamp; // HH:MM:SS
    std::string message;
};

class Logger
{
public:
    static Logger& Get();

    void Log(LogLevel level, const std::string& msg);

    void Info(const std::string& msg) { Log(LogLevel::INFO, msg); }
    void Warn(const std::string& msg) { Log(LogLevel::WARN, msg); }
    void Error(const std::string& msg) { Log(LogLevel::ERR, msg); }
    void Debug(const std::string& msg) { Log(LogLevel::DEBUG, msg); }

    // For UI rendering - returns a copy under lock
    std::vector<LogEntry> GetEntries();
    void Clear();

    // Also writes to CTTrainer_debug.log beside the .exe
    void SetFileLogging(bool enabled) { m_fileLog = enabled; }

private:
    Logger();
    ~Logger();

    std::vector<LogEntry> m_entries;
    std::mutex            m_mutex;
    bool                  m_fileLog = true;
    void* m_file = nullptr; // FILE*
};

// Convenience macros
#define LOG_INFO(msg)  Logger::Get().Info(msg)
#define LOG_WARN(msg)  Logger::Get().Warn(msg)
#define LOG_ERR(msg)   Logger::Get().Error(msg)
#define LOG_DEBUG(msg) Logger::Get().Debug(msg)