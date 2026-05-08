#include "../include/logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <ctime>

static std::mutex g_log_mutex;

void LogPrint(const std::string& tag, const std::string& msg) {
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

    std::ostringstream out;
    out << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count()
        << " " << tag << " " << msg;

    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cout << out.str() << std::endl;
}
