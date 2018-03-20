#pragma once

#include <Windows.h>
#include <string>
using namespace std;


class PipeServer {
public:
	PipeServer();
	~PipeServer();
	BOOL CreatePipeInstance();
	BOOL WaitClientConnect();
	BOOL AsyncWaitClientConnect();
	BOOL ReadData(string& outData);
	BOOL WriteData(string& inData);
	BOOL DisconnectPipe();
private:
	void Invalidate();
	BOOL InitSecurityAttributes();
private:
	HANDLE m_hPipe;
	PSID m_pEveryoneSID;
	PACL m_pACL;
	PSECURITY_DESCRIPTOR m_pSD;
	SECURITY_ATTRIBUTES m_sa;
};