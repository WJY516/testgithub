#include "Log.h"

NUJLog::NUJLog()
{
	SYSTEMTIME systime;
	GetLocalTime(&systime);

	std::string filename = "NUJ." +
		std::to_string(systime.wYear) + "-" +
		std::to_string(systime.wMonth) + "-" +
		std::to_string(systime.wDay) + "-" +
		std::to_string(systime.wHour) + "-" +
		std::to_string(systime.wMinute) + "-" +
		std::to_string(systime.wSecond) + ".log";

	file = new std::ofstream();
	file->open(filename);
}

NUJLog::~NUJLog()
{
	file->close();
	delete file;
	file = nullptr;
}

NUJLog nujLog;
