#pragma once

#include <sstream>

class Logger
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

    template<typename T>
    Logger& operator << (T&& value)
    {
        if (NeedToLog())
        {
            m_message << std::forward<T>(value);
        }

        return *this;
    }

    static void SetMinimumLevel(Level level);

private:
    bool NeedToLog() const;

private:
    Level const m_level;
    std::ostringstream m_message;
};
