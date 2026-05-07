#ifndef CORE_LOG_H
#define CORE_LOG_H

#include <string>
#include <sstream>

enum class LogLevel { START, OK, WARN, ERROR, DATA, INFO };

void CoreLog(LogLevel level, const std::string &msg, const char* file, int line, const char* func);

// 简洁易用的宏（可在编译时通过定义 DISABLE_CORE_LOG 关闭）
#ifndef DISABLE_CORE_LOG
#define LOG_RAW(level, msg) do { std::ostringstream _oss; _oss << msg; CoreLog(level, _oss.str(), __FILE__, __LINE__, __FUNCTION__); } while(0)
#define LOG_START(msg) LOG_RAW(LogLevel::START, msg)
#define LOG_OK(msg)    LOG_RAW(LogLevel::OK, msg)
#define LOG_WARN(msg)  LOG_RAW(LogLevel::WARN, msg)
#define LOG_ERR(msg)   LOG_RAW(LogLevel::ERROR, msg)
#define LOG_DATA(msg)  LOG_RAW(LogLevel::DATA, msg)
#define LOG_INFO(msg)  LOG_RAW(LogLevel::INFO, msg)
#else
#define LOG_START(msg) ((void)0)
#define LOG_OK(msg)    ((void)0)
#define LOG_WARN(msg)  ((void)0)
#define LOG_ERR(msg)   ((void)0)
#define LOG_DATA(msg)  ((void)0)
#define LOG_INFO(msg)  ((void)0)
#endif

#endif // CORE_LOG_H
