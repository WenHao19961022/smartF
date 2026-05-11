#include "../include/core_log.h"

static const char* LevelPrefix(LogLevel l) {
    switch (l) {
        case LogLevel::START: return "[START]";
        case LogLevel::OK:    return "[OK]";
        case LogLevel::WARN:  return "[WARN]";
        case LogLevel::ERROR: return "[ERROR]";
        case LogLevel::DATA:  return "[DATA]";
        case LogLevel::INFO:  return "[INFO]";
        default: return "[LOG]";
    }
}

void CoreLog(LogLevel level, const std::string &msg, [[maybe_unused]] const char* file, [[maybe_unused]] int line, const char* func) {
    std::ostringstream oss;
    oss << LevelPrefix(level) << " " << func << "() - " << msg;
    LogPrint("[Core]", oss.str());
}
