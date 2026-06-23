#pragma once
#include <iostream>
#include <string>

namespace mcobfF
{
    enum class LogLevel
    {
        Debug,
        Info,
        Warn,
        Error
    };

    class Logger
    {
    public:
        inline static auto currentLevel = LogLevel::Info;

        static void setLevel(const LogLevel level) { currentLevel = level; }

        class LogStream
        {
        public:
            LogStream(std::ostream& os, const bool enabled, const std::string& prefix)
                : os_(os), enabled_(enabled)
            {
                if (enabled_) os_ << prefix;
            }

            ~LogStream()
            {
                if (enabled_) os_ << std::endl;
            }

            LogStream(const LogStream&) = delete;
            LogStream& operator=(const LogStream&) = delete;
            LogStream(LogStream&&) = default;
            LogStream& operator=(LogStream&&) = delete;

            template <typename T>
            LogStream& operator<<(const T& val)
            {
                if (enabled_) os_ << val;
                return *this;
            }

            LogStream& operator<<(std::ostream& (*manip)(std::ostream&))
            {
                if (enabled_) manip(os_);
                return *this;
            }

        private:
            std::ostream& os_;
            bool enabled_;
        };

        static LogStream debug(const std::string& tag)
        {
            return LogStream(std::cout, currentLevel <= LogLevel::Debug,
                             "[" + tag + "] [DEBUG] ");
        }

        static LogStream info(const std::string& tag)
        {
            return LogStream(std::cout, currentLevel <= LogLevel::Info,
                             "[" + tag + "] ");
        }

        static LogStream warn(const std::string& tag)
        {
            return LogStream(std::cout, currentLevel <= LogLevel::Warn,
                             "[" + tag + "] [WARN] ");
        }

        static LogStream error(const std::string& tag)
        {
            return LogStream(std::cout, currentLevel <= LogLevel::Error,
                             "[" + tag + "] [ERROR] ");
        }
    };
} // namespace mcobfF
