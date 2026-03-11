// ============================================================
//  Logger.cpp
// ============================================================
#include "../include/Logger.h"
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
        if (f) fprintf(f, "=== CTTrainer Debug Log ===\n\n");
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
    std::lock_guard<std::mutex> lock(m_mutex);

    LogEntry e{ level, msg };
    m_entries.push_back(e);

    // Keep last 500 lines in memory
    if (m_entries.size() > 500)
        m_entries.erase(m_entries.begin());

    // Write to file
    if (m_file)
    {
        fprintf((FILE*)m_file, "%s%s\n", LevelStr(level), msg.c_str());
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