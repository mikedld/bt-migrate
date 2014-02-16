#include "Logger.h"

#include <boost/date_time.hpp>

#include <iostream>
#include <mutex>

namespace pt = boost::posix_time;

namespace
{

std::mutex LogFlushMutex;

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
    m_level(level)
{
    //
}

Logger::~Logger()
{
    std::lock_guard<std::mutex> lock(LogFlushMutex);
    std::cout << "[" << pt::to_simple_string(pt::microsec_clock::local_time()) << "] [" << LevelToString(m_level) << "] " <<
        str() << std::endl;
}
