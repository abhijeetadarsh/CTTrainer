// ============================================================
//  Logger.cpp
// ============================================================
#include "Logger.h"

#include <cstdio>
#include <ctime>
#include <Windows.h>

static const char* LevelStr(LogLevel l)
{
    switch (l)
    {
    case LogLevel::INFO:  return "[INFO] ";
    case LogLevel::WARN:  return "[WARN] ";
    case LogLevel::ERR:   return "[ERR]  ";
    case LogLevel::DEBUG: return "[DBG]  ";
    }
    return "[?]    ";
}

Logger::Logger()
{
    if (m_fileLog)
    {
        // Place log file next to the .exe
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        // Replace filename with CTTrainer_debug.log
        char* slash = strrchr(path, '\\');
        if (slash) *(slash + 1) = '\0';
        strcat_s(path, "CTTrainer_debug.log");

        FILE* f = nullptr;
        fopen_s(&f, path, "w");
        m_file = f;
        if (f)
        {
            time_t now = time(nullptr);
            tm  lt = {};
            localtime_s(&lt, &now);
            char dateBuf[64];
            strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d %H:%M:%S", &lt);
            fprintf(f, "=== CTTrainer Debug Log  [started %s] ===\n\n", dateBuf);
        }
    }
}

Logger::~Logger()
{
    if (m_file) fclose((FILE*)m_file);
}

Logger& Logger::Get()
{
    static Logger instance;
    return instance;
}

void Logger::Log(LogLevel level, const std::string& msg)
{
    // Capture timestamp before taking the lock
    time_t now = time(nullptr);
    tm lt = {};
    localtime_s(&lt, &now);
    char tsBuf[16];
    snprintf(tsBuf, sizeof(tsBuf), "%02d:%02d:%02d",
        lt.tm_hour, lt.tm_min, lt.tm_sec);

    std::lock_guard<std::mutex> lock(m_mutex);

    LogEntry e;
    e.level     = level;
    e.timestamp = tsBuf;
    e.message   = msg;
    m_entries.push_back(e);

    // Keep last 500 lines in memory
    if (m_entries.size() > 500)
        m_entries.erase(m_entries.begin());

    // Write to file with timestamp
    if (m_file)
    {
        fprintf((FILE*)m_file, "%s %s%s\n", tsBuf, LevelStr(level), msg.c_str());
        fflush((FILE*)m_file);
    }
}

std::vector<LogEntry> Logger::GetEntries()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries;
}

void Logger::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}