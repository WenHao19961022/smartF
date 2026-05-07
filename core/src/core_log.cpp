#include "../include/core_log.h"

#include <iostream>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <thread>
#include <ctime>

static std::mutex g_log_mutex;

static const char* LevelPrefix(LogLevel l) {
    switch (l) {
        case LogLevel::START: return "🚀 [➔ START]";
        case LogLevel::OK:    return "✅ [✓ OK]";
        case LogLevel::WARN:  return "⚠️ [⚠ WARN]";
        case LogLevel::ERROR: return "❌ [❌ ERROR]";
        case LogLevel::DATA:  return "📊 [DATA]";
        case LogLevel::INFO:  return "🔔 [INFO]";
        default: return "[LOG]";
    }
}

void CoreLog(LogLevel level, const std::string &msg, const char* file, int line, const char* func) {
    using namespace std::chrono;

    auto now = system_clock::now();
    auto tt = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_buf;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%F %T") << "." << std::setfill('0') << std::setw(3) << ms.count();

    std::ostringstream out;
    out << oss.str() << " " << LevelPrefix(level) << " " << func << "() - " << msg
        << " (" << file << ":" << line << ")";

    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cout << out.str() << std::endl;
}
