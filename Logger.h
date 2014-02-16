#pragma once

#include <sstream>

class Logger : public std::ostringstream
{
public:
    enum Level
    {
        Debug,
        Info,
        Warning,
        Error
    };

public:
    Logger(Level level);
    ~Logger();

private:
    Level const m_level;
};
