#include "PipeServer.h"
#include "NtfsUsnJournal.h"
#include "Log.h"
#include <regex>
#include <tchar.h>
using namespace std;
#pragma comment( linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" ) // 设置连接器选项 

int main(int argc, char* agrs[])
{
	PipeServer pipeserver;
	NtfsUsnJournal usnJournal;
	string drvname;
	string rootdir;

	LOG_INFO<< "start service\n";

	if (pipeserver.CreatePipeInstance() == FALSE)
	{
		LOG_ERROR << "create pipe instance failed\n";
		return 0;
	}

	if (pipeserver.WaitClientConnect() == FALSE)
	{
		LOG_ERROR << "WaitClientConnect failed\n";
		return 0;
	}

	while (TRUE)
	{

		string command;
		string retMsg;
		regex  start("(startNtfsJournal:)[A-Z]:[0-9]+");

		if (pipeserver.ReadData(command) == FALSE)
		{
			LOG_ERROR << "ReadData failed\n";
			break;
		}
		if (regex_match(command, start))
		{
			
			drvname = command.substr(strlen("startNtfsJournal")+1, 1);
			rootdir = command.substr(strlen("startNtfsJournal") + 3, command.size() - strlen("startNtfsJournal") - 3);
			LOG_INFO << "start monitor drive " << drvname<<"\n";
			usnJournal.SetRootDir(rootdir);
			usnJournal.CreateUsnJournal(drvname);
			retMsg = "success";
		}
		else if (!command.compare("stopNtfsJournal"))
		{
			LOG_INFO << "stop monitor drive " << drvname << "\n";
			usnJournal.FilterUsnJournal();
			retMsg = "success";
		}
		else
		{
			retMsg = "unknown command";
			LOG_WARN << "unknown command:" << command << "\n";
		}
		if (pipeserver.WriteData(retMsg) == FALSE)
		{
			LOG_ERROR << "WriteData failed\n";
			break;
		}
	}
	LOG_INFO << "stop service \n";
	return 0;
}