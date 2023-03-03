#include "Logger.h"

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <chrono>
#include <iostream>
#include <mutex>

namespace
{

std::mutex LogFlushMutex;
Logger::Level MinimumLevel = Logger::Info;

std::string LevelToString(Logger::Level level)
{
    switch (level)
    {
    case Logger::Debug:
        return "D";
    case Logger::Info:
        return "I";
    case Logger::Warning:
        return "W";
    case Logger::Error:
        return "E";
    }

    return "?";
}

} // namespace

Logger::Logger(Level level) :
    m_level(level),
    m_message()
{
    //
}

Logger::~Logger()
{
    if (!NeedToLog())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(LogFlushMutex);
    fmt::print("[{:%F %T}] [{}] {}\n", std::chrono::system_clock::now(), LevelToString(m_level), m_message.str());
}

void Logger::SetMinimumLevel(Level level)
{
    MinimumLevel = level;
}

bool Logger::NeedToLog() const
{
    return m_level >= MinimumLevel;
}
