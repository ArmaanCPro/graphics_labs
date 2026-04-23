#pragma once

#ifndef ENABLE_LOGGING
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_OFF
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <memory>
#include <array>

namespace enger
{
    class Logger
    {
        static constexpr auto defaultLoggerName = "enger";
    public:
        static void init()
        {
            const auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_level(spdlog::level::trace);
            consoleSink->set_pattern("%^[%T] [%n] [%@]: %v%$"); // [Time] [LoggerName] [File:Line] [Level]: Message

            static constexpr auto maxSize = 1024 * 1024 * 5; // 5 MB (per file)
            static constexpr auto maxFiles = 3; // max 3 files
            const auto rotatingFileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/enger.log", maxSize, maxFiles);
            rotatingFileSink->set_level(spdlog::level::trace);
            rotatingFileSink->set_pattern("[%T] [%n] [%@ %l]: %v"); // [Time] [LoggerName] [File:Line type] [Level]: Message

            std::array<spdlog::sink_ptr, 2> sinks{consoleSink, rotatingFileSink};
            spdlog::init_thread_pool(8192, 1);
            const auto logger = std::make_shared<spdlog::async_logger>(defaultLoggerName, sinks.begin(), sinks.end(),
                spdlog::thread_pool(), spdlog::async_overflow_policy::block);

            logger->set_level(spdlog::level::trace);
            spdlog::register_logger(logger);
            spdlog::set_default_logger(logger);
        }

        inline static spdlog::logger* get()
        {
            return spdlog::get(defaultLoggerName).get();
        }
    };

#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(enger::Logger::get(), __VA_ARGS__)
#define LOG_INFO(...)  SPDLOG_LOGGER_INFO(enger::Logger::get(), __VA_ARGS__)
#define LOG_WARN(...)  SPDLOG_LOGGER_WARN(enger::Logger::get(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(enger::Logger::get(), __VA_ARGS__)
#define LOG_FATAL(...) SPDLOG_LOGGER_CRITICAL(enger::Logger::get(), __VA_ARGS__)
}
