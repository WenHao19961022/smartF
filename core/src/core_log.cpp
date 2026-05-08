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

void CoreLog(LogLevel level, const std::string &msg, const char* file, int line, const char* func) {
    std::ostringstream oss;
    oss << LevelPrefix(level) << " " << func << "() - " << msg;
    LogPrint("[Core]", oss.str());
}
