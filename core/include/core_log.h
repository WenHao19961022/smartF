#ifndef CORE_LOG_H
#define CORE_LOG_H

#include <string>
#include <sstream>

// RAII 风格的作用域追踪，构造时发送 START，析构时若未明确结束则发 WARN
class LogScope {
public:
	LogScope(const char* func, const char* file, int line)
		: m_func(func ? func : ""), m_file(file ? file : ""), m_line(line) {
		CoreLog(LogLevel::START, "", m_file.c_str(), m_line, m_func.c_str());
	}

	~LogScope() {
		// 如果当前线程的记录函数仍为本函数，说明没有 LOG_OK/LOG_ERR 被调用
		std::string curr = CoreGetCurrentFunction();
		if (curr == m_func) {
			CoreLog(LogLevel::WARN, "Exited without OK", m_file.c_str(), m_line, m_func.c_str());
		}
	}

	// 禁止拷贝
	LogScope(const LogScope&) = delete;
	LogScope& operator=(const LogScope&) = delete;

private:
	std::string m_func;
	std::string m_file;
	int m_line;
};

// 便捷宏：在函数最顶部插入一行即可
#define LOG_TRACE_SCOPE() LogScope _log_scope_obj(__FUNCTION__, __FILE__, __LINE__)

enum class LogLevel { START, OK, WARN, ERROR, DATA, INFO };

void CoreLog(LogLevel level, const std::string &msg, const char* file, int line, const char* func);
// 读取当前正在运行（最近由 LOG_START 标记）的函数名
std::string CoreGetCurrentFunction();

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
