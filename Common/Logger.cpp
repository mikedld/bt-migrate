#include "Logger.h"

#include "Util.h"

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
    std::cout << "[" << Util::GetTimestamp() << "] [" << LevelToString(m_level) << "] " << m_message.str() << std::endl;
}

void Logger::SetMinimumLevel(Level level)
{
    MinimumLevel = level;
}

bool Logger::NeedToLog() const
{
    return m_level >= MinimumLevel;
}
