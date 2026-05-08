#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <sstream>

// 线程安全的统一打印函数
// 用法: LogPrint("[Core]", "message");
void LogPrint(const std::string& tag, const std::string& msg);

// 宏: 构造 ostringstream 并调用 LogPrint
#define LOG_PRINT(tag, msg) do { std::ostringstream _oss; _oss << msg; LogPrint(tag, _oss.str()); } while(0)

#endif // LOGGER_H
