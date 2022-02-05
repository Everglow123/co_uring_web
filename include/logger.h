//# -*- encoding: utf-8 -*-
//'''
//@文件    :logger.h
//@时间    :2021/11/27 22:27:32
//@作者    :周恒
//@版本    :1.0
//@说明    :日志模块，日志应该是每个线程都可以写，使用dup2来进行rotate
//'''
//
#pragma once

#include <bits/types/struct_iovec.h>
#include <pthread.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "config.h"
namespace co_uring_web::utils {
class GlobalLoggerManager {
	static GlobalLoggerManager *instance;//单例，需要手动初始化
	int loggerFileFd_;
	std::thread *loggerThread;//日志线程

   public:
	static void init();
	inline int getLoggerFileFd() noexcept { return loggerFileFd_; }
	bool checkNeedRotate();

   private:
	/**
	 * @brief 没有扩展名
	 *
	 */
	std::string fileName_;
	std::string outputDir_;
	static constexpr const char *outputFileExtensionName = "log";

	/**
	 * @brief 日志滚动，此函数目的是，当当前写入的日志文件{outputDir_/fileName_.log}的大小超过一个g之后，把这个文件重名成{outputDir_/fileName_.log1}
	 * 后面的文件以此类推，同时新建一个日志文件，并用dup2操作,把文件描述符loggerFileFd_重定向到新文件中
	 *
	 */
	void rotate();

   public:
	inline std::string getOutputDir() { return outputDir_; }
	inline std::string getFileName() { return fileName_; }
	GlobalLoggerManager()=default;
	GlobalLoggerManager(const GlobalLoggerManager &) = delete;
	GlobalLoggerManager(GlobalLoggerManager &&) = delete;
	static GlobalLoggerManager &getInstance();
};
enum LogLevel { TRACE, DEBUG, INFO, WARN, ERROR, FATAL };

class LoggerInfoBuilder {
   public:
	static constexpr const char *LogLevelName[6] = {"TRACE ", "DEBUG ", "INFO  ",
	                                                "WARN  ", "ERROR ", "FATAL "};
	static constexpr int logInfoMaxSize = 512;                  //日志信息最大长度
	static constexpr int timeHeaderLength = 20;                 //日志信息固定头部长度
	thread_local static char timeHeader[timeHeaderLength + 1];  //日志头 每秒构建一次
	thread_local static time_t threadTime;  //线程本地的时间戳，以秒为单位
	thread_local static pthread_t threadId; 
	/**
	 * @brief iovecs_[0]是日志信息固定头部,iovecs_[1]才是具体的日志信息
	 * 
	 */
	iovec iovecs_[2] = {{nullptr, 0}, {nullptr, 0}};
	LoggerInfoBuilder() = default;
	~LoggerInfoBuilder() { free(iovecs_[1].iov_base); }
	void format(LogLevel level, const char *file, uint line, const char *func) {
		using namespace std;
		time_t t = std::time(nullptr);
		if (!threadId) {
			threadId = pthread_self();
		}
		if (t != LoggerInfoBuilder::threadTime) {
			tm lt;
			localtime_r(&t, &lt);
			LoggerInfoBuilder::threadTime = t;
			std::strftime(LoggerInfoBuilder::timeHeader, sizeof(LoggerInfoBuilder::timeHeader),
			              "%F %T:", &lt);
		}
		// memcpy(this->iovecs_[1].iov_base, timeHeader, timeHeaderLength);
		iovecs_[0].iov_base = timeHeader;
		iovecs_[0].iov_len = timeHeaderLength;
		iovecs_[1].iov_base = malloc(logInfoMaxSize);
		iovecs_[1].iov_len += snprintf(
		    (char *)iovecs_[1].iov_base, logInfoMaxSize,
		    "%.3lld [ %s - %s - %u ] [thread %lu] %s: ",
		    (chrono::high_resolution_clock::now().time_since_epoch().count() / 1000000) % 1000,
		    file, func, line, threadId, LogLevelName[(int)level]);
	}
	void build(const std::string_view view) {
		using namespace std;
		if (view.size() + 1 > logInfoMaxSize - iovecs_[1].iov_len) {
			memcpy((char *)iovecs_[1].iov_base + iovecs_[1].iov_len, view.data(),
			       logInfoMaxSize - iovecs_[1].iov_len);
			iovecs_[1].iov_len = logInfoMaxSize;
		} else {
			memcpy((char *)iovecs_[1].iov_base + iovecs_[1].iov_len, view.data(), view.size());
			iovecs_[1].iov_len += view.size();
		}
		return;
	}
	void build(const std::string &s) { return build(std::string_view(s)); }
	void build(const char *str) {
		using namespace std;
		auto size = strlen(str);
		return build(string_view {str, size});
	}
	void build(const std::exception &ex) {
		return build(std::string_view {ex.what(), strlen(ex.what())});
	}
	void build(int32_t num) {
		iovecs_[1].iov_len += snprintf((char *)iovecs_[1].iov_base + iovecs_[1].iov_len,
		                               logInfoMaxSize - iovecs_[1].iov_len, "%d", num);
		return;
	}
	void build(uint32_t num) {
		iovecs_[1].iov_len += snprintf((char *)iovecs_[1].iov_base + iovecs_[1].iov_len,
		                               logInfoMaxSize - iovecs_[1].iov_len, "%u", num);
		return;
	}
	void build(int64_t num) {
		iovecs_[1].iov_len += snprintf((char *)iovecs_[1].iov_base + iovecs_[1].iov_len,
		                               logInfoMaxSize - iovecs_[1].iov_len, "%ld", num);
		return;
	}
	void build(uint64_t num) {
		iovecs_[1].iov_len += snprintf((char *)iovecs_[1].iov_base + iovecs_[1].iov_len,
		                               logInfoMaxSize - iovecs_[1].iov_len, "%lu", num);
		return;
	}
	void build(float num){
		iovecs_[1].iov_len += snprintf((char *)iovecs_[1].iov_base + iovecs_[1].iov_len,
		                               logInfoMaxSize - iovecs_[1].iov_len, "%f", num);
		return;
	}
	void build(double num){
		iovecs_[1].iov_len += snprintf((char *)iovecs_[1].iov_base + iovecs_[1].iov_len,
		                               logInfoMaxSize - iovecs_[1].iov_len, "%lf", num);
		return;
	}
	void build(char c) {
		if (iovecs_[1].iov_len < logInfoMaxSize)
			((char *)iovecs_[1].iov_base)[iovecs_[1].iov_len++] = c;
		return;
	}
};
/**
 * @brief 日志等级为ERROR 或者 FATAL的，写完后将会立即flush
 * 单个日志大小最大为1GB左右
 *
 */
class Logger {
	LoggerInfoBuilder builder_;
	LogLevel level_;

   public:
	static constexpr size_t singleFileSizeLimit = 1<<30;
	void checkFileSize() {}

   public:
	friend LoggerInfoBuilder;

	Logger(LogLevel level, const char *file, int line, const char *func) : level_(level) {
		builder_.format(level, file, line, func);
	}
	template <class T>
	Logger &operator<<(const T &data) {
		builder_.build(data);
		return *this;
	}
	// template<size_t Strlen>
	// Logger &operator<< (const char (&arr) [Strlen] ){
	// 	builder_.build(std::string_view{arr,Strlen});
	// }

	// Logger &operator<<(std::string &&data) {

	// };
	/**
	 * @brief Destroy the Logger object
	 *
	 */
	~Logger() {
		int len=std::min((int)builder_.iovecs_[1].iov_len,LoggerInfoBuilder::logInfoMaxSize-1);
		((char*)(builder_.iovecs_[1].iov_base))[len]='\n';
		builder_.iovecs_[1].iov_len++;
		writev(GlobalLoggerManager::getInstance().getLoggerFileFd(), builder_.iovecs_, 2);
		if (level_ > WARN) {
			fsync(GlobalLoggerManager::getInstance().getLoggerFileFd());
		}
		free(builder_.iovecs_[1].iov_base);
	}
};

}  // namespace co_uring_web::utils
#define LOG_TRACE (co_uring_web::utils::Logger(co_uring_web::utils::LogLevel::TRACE, __FILE__, __LINE__, __func__))  // NOLINT
#define LOG_DEBUG (co_uring_web::utils::Logger(co_uring_web::utils::LogLevel::DEBUG, __FILE__, __LINE__, __func__))  // NOLINT
#define LOG_INFO (co_uring_web::utils::Logger(co_uring_web::utils::LogLevel::INFO, __FILE__, __LINE__, __func__))    // NOLINT
#define LOG_WARN (co_uring_web::utils::Logger(co_uring_web::utils::LogLevel::WARN, __FILE__, __LINE__, __func__))    // NOLINT
#define LOG_ERROR (co_uring_web::utils::Logger(co_uring_web::utils::LogLevel::ERROR, __FILE__, __LINE__, __func__))  // NOLINT
#define LOG_FATAL (co_uring_web::utils::Logger(co_uring_web::utils::LogLevel::FATAL, __FILE__, __LINE__, __func__))