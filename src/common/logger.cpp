#include "logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>

std::shared_ptr<spdlog::logger> Logger::logger_;

void Logger::init() {
    logger_ = spdlog::stdout_color_mt("visa_gateway");
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
}

std::shared_ptr<spdlog::logger> Logger::get() {
    return logger_;
}
