#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <windows.h>

#define LOG_PREFIX		{	SYSTEMTIME systime; \
								GetLocalTime(&systime); \
								nujLog << std::to_string(systime.wHour) + "h:" + \
								std::to_string(systime.wMinute) + "m:" + \
								std::to_string(systime.wSecond) + "s:" + \
								std::to_string(systime.wMilliseconds) + "ms		" + \
								__FUNCTION__ << "		"; }

#define LOG_TRACE	LOG_PREFIX \
								nujLog << "TRACE:		"

#define LOG_DEBUG	LOG_PREFIX \
								nujLog << "DEBUG:		"

#define LOG_INFO	LOG_PREFIX \
								nujLog << "INFO:		"

#define LOG_WARN	LOG_PREFIX \
								nujLog << "WARN:		"

#define LOG_ERROR	LOG_PREFIX \
								nujLog << "ERROR:		"


class NUJLog
{
private:
	std::ofstream* file;
	std::mutex mtx;

public:
	NUJLog();
	~NUJLog();

	template <typename T>
	NUJLog& operator<<(const T& x)
	{
		std::lock_guard<std::mutex> lock(mtx);
		(*file) << x;
		(*file).flush();
		return *this;
	}

	// for endl operation
	NUJLog& operator<<(std::ostream& (*os)(std::ostream&))
	{
		std::lock_guard<std::mutex> lock(mtx);
		(*file) << os;
		(*file).flush();
		return *this;
	}
};

extern NUJLog nujLog;
