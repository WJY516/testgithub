#include "PipeServer.h"
#include "Log.h"
#include <stdio.h>
#include <aclapi.h>
#include <tchar.h>
#pragma comment(lib, "advapi32.lib")

PipeServer::PipeServer() :m_hPipe(NULL)
{
	InitSecurityAttributes();
}

PipeServer::~PipeServer()
{
	if (m_pEveryoneSID)
		FreeSid(m_pEveryoneSID);
	if (m_pACL)
		LocalFree(m_pACL);
	if (m_pSD)
		LocalFree(m_pSD);

}

BOOL PipeServer::InitSecurityAttributes()
{
	BOOL ret = FALSE;
	DWORD dwRes, dwDisposition;
	EXPLICIT_ACCESS ea[1];
	SID_IDENTIFIER_AUTHORITY SIDAuthWorld =
		SECURITY_WORLD_SID_AUTHORITY;
	SID_IDENTIFIER_AUTHORITY SIDAuthNT = SECURITY_NT_AUTHORITY;

	// Create a well-known SID for the Everyone group.
	if (!AllocateAndInitializeSid(&SIDAuthWorld, 1,
		SECURITY_WORLD_RID,
		0, 0, 0, 0, 0, 0, 0,
		&m_pEveryoneSID))
	{
		_tprintf(_T("AllocateAndInitializeSid Error %u\n"), GetLastError());
		goto Cleanup;
	}

	// Initialize an EXPLICIT_ACCESS structure for an ACE.
	// The ACE will allow Everyone read access to the key.
	ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
	ea[0].grfAccessPermissions = GENERIC_ALL;
	ea[0].grfAccessMode = SET_ACCESS;
	ea[0].grfInheritance = NO_INHERITANCE;
	ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
	ea[0].Trustee.ptstrName = (LPTSTR)m_pEveryoneSID;

	// Create a new ACL that contains the new ACEs.
	dwRes = SetEntriesInAcl(1, ea, NULL, &m_pACL);
	if (ERROR_SUCCESS != dwRes)
	{
		_tprintf(_T("SetEntriesInAcl Error %u\n"), GetLastError());
		goto Cleanup;
	}

	// Initialize a security descriptor.  
	m_pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR,
		SECURITY_DESCRIPTOR_MIN_LENGTH);
	if (NULL == m_pSD)
	{
		_tprintf(_T("LocalAlloc Error %u\n"), GetLastError());
		goto Cleanup;
	}

	if (!InitializeSecurityDescriptor(m_pSD,
		SECURITY_DESCRIPTOR_REVISION))
	{
		_tprintf(_T("InitializeSecurityDescriptor Error %u\n"),
			GetLastError());
		goto Cleanup;
	}

	// Add the ACL to the security descriptor. 
	if (!SetSecurityDescriptorDacl(m_pSD,
		TRUE,     // bDaclPresent flag   
		m_pACL,
		FALSE))   // not a default DACL 
	{
		_tprintf(_T("SetSecurityDescriptorDacl Error %u\n"),
			GetLastError());
		goto Cleanup;
	}

	// Initialize a security attributes structure.
	m_sa.nLength = sizeof (SECURITY_ATTRIBUTES);
	m_sa.lpSecurityDescriptor = m_pSD;
	m_sa.bInheritHandle = FALSE;

	ret = TRUE;

Cleanup:
	return ret;

}

BOOL PipeServer::CreatePipeInstance()
{
	BOOL ret = FALSE;
	// 创建命名管道
	m_hPipe = CreateNamedPipe("\\\\.\\Pipe\\NtfsJournal", PIPE_ACCESS_DUPLEX /*| FILE_FLAG_OVERLAPPED*/, 0, PIPE_UNLIMITED_INSTANCES, 0, 0, 0, &m_sa);
	if (m_hPipe == INVALID_HANDLE_VALUE)
	{
		LOG_ERROR << "create named pipe failed—— error : " << GetLastError() << "\n";
	}
	else
	{
		LOG_INFO<< "create named pipe success!\n";
		ret = TRUE;
	}
	return ret;
}

BOOL PipeServer::WaitClientConnect()
{
	if (!ConnectNamedPipe(m_hPipe, NULL))
	{
		DWORD err = GetLastError();
		if (ERROR_IO_PENDING != GetLastError())
		{
			CloseHandle(m_hPipe);
			LOG_ERROR << "ConnectNamedPipe failed—— error : " << GetLastError() << "\n";

			return FALSE;
		}

	}
	return TRUE;
}

BOOL PipeServer::AsyncWaitClientConnect()
{
	HANDLE                    hEvent;
	OVERLAPPED                ovlpd;

	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!hEvent)
	{
		LOG_ERROR << "CreateEvent failed—— error : " << GetLastError() << "\n";
		return FALSE;
	}

	memset(&ovlpd, 0, sizeof(OVERLAPPED));
	ovlpd.hEvent = hEvent;

	LOG_INFO << "listen client connect\n";
	if (!ConnectNamedPipe(m_hPipe, &ovlpd))
	{
		if (ERROR_IO_PENDING != GetLastError())
		{
			CloseHandle(m_hPipe);
			CloseHandle(hEvent);
			LOG_ERROR << "ConnectNamedPipe failed—— error : " << GetLastError() << "\n";

			return FALSE;
		}

	}

	if (WAIT_FAILED == WaitForSingleObject(hEvent, INFINITE))
	{
		CloseHandle(m_hPipe);
		CloseHandle(hEvent);
		LOG_ERROR << "WaitForSingleObject failed—— error : " << GetLastError() << "\n";

		return FALSE;
	}
	CloseHandle(hEvent);
	return TRUE;
}

void PipeServer::Invalidate()
{

}

BOOL PipeServer::ReadData(string& outData)
{
	char buffer[1024] = { 0 };
	DWORD ReadNum;
	if (ReadFile(m_hPipe, buffer, sizeof(buffer), &ReadNum, NULL) == FALSE)
	{
		CloseHandle(m_hPipe);
		LOG_ERROR << "ReadData from pipe failed—— error : " << GetLastError() << "\n";
		Invalidate();
		return FALSE;
	}
	else {
		buffer[ReadNum] = '\0'; 
		outData = buffer;
		LOG_INFO << buffer << "\n";
		Invalidate();
	}
	return TRUE;
}

BOOL PipeServer::WriteData(string& inData)
{
	char buffer[1024] = { 0 };
	DWORD WriteNum;
	
	strcpy(buffer, inData.c_str());
	buffer[inData.size()] = '\0';
	if (WriteFile(m_hPipe, buffer, inData.size(), &WriteNum, NULL) == FALSE)
	{
		CloseHandle(m_hPipe); 
		LOG_ERROR << "WriteData to pipe failed—— error : " << GetLastError() << "\n";
		Invalidate();
		return FALSE;
	}
	else {
		buffer[WriteNum] = '\0';
		LOG_INFO << buffer << "\n";
		Invalidate();
	}
	return TRUE;
}

BOOL PipeServer::DisconnectPipe()
{
	string m_sMessage;
	if (DisconnectNamedPipe(m_hPipe) == FALSE)
	{
		LOG_ERROR << "DisconnectNamedPipe failed—— error : " << GetLastError() << "\n";
		return FALSE;
	}
	else
	{
		CloseHandle(m_hPipe); // 关闭管道句柄
		LOG_INFO<< "DisconnectNamedPipe successed\n";
	}
	return TRUE;
}