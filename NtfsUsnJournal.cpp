#include "NtfsUsnJournal.h"
#include "EventFilter.h"
#include "Log.h"
using namespace std;

#define USN_JOURNAL_LOG_NAME "ntfsjournal.json"

wstring StringtoWstring(const string str)
{
	// string转wstring
	UINT nCodePage = CP_UTF8; //UTF-8
	std::wstring wstr;

	int nLength = MultiByteToWideChar(nCodePage, 0, str.c_str(), -1, NULL, 0);

	wchar_t* pBuffer = new wchar_t[nLength + 1];

	MultiByteToWideChar(nCodePage, 0, str.c_str(), -1, pBuffer, nLength);

	pBuffer[nLength] = 0;

	wstr = pBuffer;

	delete pBuffer;

	return wstr;
}

string WStringToString(const wstring wstr)
{
	string result;
	setlocale(LC_CTYPE, "");
	const wchar_t* _Source = wstr.c_str();
	size_t _Dsize = 2 * wstr.size() + 1;
	char *_Dest = new char[_Dsize];
	memset(_Dest, 0, _Dsize);
	wcstombs(_Dest, _Source, _Dsize);
	result = _Dest;
	delete[]_Dest;

	return result;
}


NtfsUsnJournal::NtfsUsnJournal() :m_hVol(INVALID_HANDLE_VALUE)
{
	NtCreatefile = (PNtCreateFile)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtCreateFile");
	NtQueryInformationFile = (PNtQueryInformationFile)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtQueryInformationFile");
}

NtfsUsnJournal::~NtfsUsnJournal()
{

}

void NtfsUsnJournal::EnablePrivilege()
{
	LPCTSTR arPrivelegeNames[] = {
		SE_BACKUP_NAME, //	these two are required for the FILE_FLAG_BACKUP_SEMANTICS flag used in the call to 
		SE_RESTORE_NAME,//  CreateFile() to open the directory handle for ReadDirectoryChangesW

		SE_CHANGE_NOTIFY_NAME //just to make sure...it's on by default for all users.
		//<others here as needed>
	};
	for (int i = 0; i < sizeof(arPrivelegeNames) / sizeof(arPrivelegeNames[0]); ++i)
	{
		BOOL fOk = FALSE;
		// Assume function fails    
		HANDLE hToken;
		// Try to open this process's access token    
		if (OpenProcessToken(GetCurrentProcess(),
			TOKEN_ADJUST_PRIVILEGES, &hToken))
		{
			// privilege        
			TOKEN_PRIVILEGES tp = { 1 };

			if (LookupPrivilegeValue(NULL, arPrivelegeNames[i], &tp.Privileges[0].Luid))
			{
				tp.Privileges[0].Attributes = TRUE ? SE_PRIVILEGE_ENABLED : 0;

				AdjustTokenPrivileges(hToken, FALSE, &tp,
					sizeof(tp), NULL, NULL);

				fOk = (GetLastError() == ERROR_SUCCESS);
			}
			CloseHandle(hToken);
		}
	}
}

BOOL NtfsUsnJournal::CreateUsnJournal(string drvname)
{
	bool ret = false;

	char FileSystemName[MAX_PATH + 1];
	DWORD MaximumComponentLength;

	m_drive = drvname;
	m_usnlogpath = drvname + ":\\" + USN_JOURNAL_LOG_NAME;

	EnablePrivilege();

	if (GetVolumeInformationA((std::string(drvname) + ":\\").c_str(), 0, 0, 0, &MaximumComponentLength, 0, FileSystemName, MAX_PATH + 1)
		&& 0 == strcmp(FileSystemName, "NTFS")) // 判断是否为 NTFS 格式
	{
		m_hVol = CreateFileA((std::string("\\\\.\\") + drvname + ":").c_str() // 需要管理员权限，无奈
			, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (m_hVol != INVALID_HANDLE_VALUE)
		{
			DWORD br;
			CREATE_USN_JOURNAL_DATA cujd;
			cujd.MaximumSize = 0; // 0表示使用默认值
			cujd.AllocationDelta = 0; // 0表示使用默认值
			int status = DeviceIoControl(m_hVol,
				FSCTL_CREATE_USN_JOURNAL,
				&cujd,
				sizeof(cujd),
				NULL,
				0,
				&br,
				NULL);

			if (0 != status){
				LOG_INFO<<"Create USN JOURNAL successed\n";
			}
			else{
				LOG_ERROR << "Create USN JOURNAL failed —— status:" << status << " error : " << GetLastError() << "\n";
			}


			//CloseHandle( hVol );
		}
		else
		{
			LOG_ERROR << "Open Volume " << drvname << " failed —— error : " << GetLastError() << "\n";
		}
	}

	return ret;
}

BOOL NtfsUsnJournal::DeleteUsnJournal()
{
	BOOL ret = FALSE;
	int status;
	DWORD br;
	DELETE_USN_JOURNAL_DATA dujd;
	dujd.UsnJournalID = m_qujd.UsnJournalID;
	dujd.DeleteFlags = USN_DELETE_FLAG_DELETE;

	status = DeviceIoControl(m_hVol,
		FSCTL_DELETE_USN_JOURNAL,
		&dujd,
		sizeof(dujd),
		NULL,
		0,
		&br,
		NULL);

	if (0 != status){
		LOG_INFO << "Delete USN JOURNAL successed\n";
		ret = TRUE;
	}
	else{
		LOG_ERROR << "Delete USN JOURNAL failed —— status:" << status << " error : " << GetLastError() << "\n";
	}

	if (m_hVol != INVALID_HANDLE_VALUE)
		CloseHandle(m_hVol);

	return ret;
}

BOOL NtfsUsnJournal::EnumUsnRecord(std::deque<ORG_USN_RECORD>& con)
{
	BOOL ret = FALSE;
	DWORD br;
	if (m_hVol == INVALID_HANDLE_VALUE)
	{
		return ret;
	}
	if (DeviceIoControl(m_hVol, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &m_qujd, sizeof(m_qujd), &br, NULL))
	{
		char buffer[0x1000];
		DWORD BytesReturned;
		READ_USN_JOURNAL_DATA rujd = { 0, -1, 0, 0, 0, m_qujd.UsnJournalID };
		for (; DeviceIoControl(m_hVol, FSCTL_READ_USN_JOURNAL, &rujd, sizeof(rujd), buffer, _countof(buffer), &BytesReturned, NULL); rujd.StartUsn = *(USN*)&buffer)
		{
			DWORD dwRetBytes = BytesReturned - sizeof(USN);
			PUSN_RECORD UsnRecord = (PUSN_RECORD)((PCHAR)buffer + sizeof(USN));
			if (dwRetBytes == 0)
			{
				ret = TRUE;
				break;
			}

			while (dwRetBytes > 0)
			{
				ORG_USN_RECORD myur = { UsnRecord->FileReferenceNumber, UsnRecord->ParentFileReferenceNumber, UsnRecord->TimeStamp, UsnRecord->Reason, UsnRecord->FileAttributes, UsnRecord->FileNameLength };
				memcpy(myur.FileName, UsnRecord->FileName, UsnRecord->FileNameLength);
				myur.FileName[UsnRecord->FileNameLength / 2] = L'\0';

				con.push_back(myur);

				dwRetBytes -= UsnRecord->RecordLength;
				UsnRecord = (PUSN_RECORD)((PCHAR)UsnRecord + UsnRecord->RecordLength);
			}
		}

	}
	else
	{
		LOG_ERROR << "Query USN JOURNAL failed —— error : " << GetLastError() << "\n";
	}

	ret = DeleteUsnJournal();
	return ret;
}


BOOL NtfsUsnJournal::FilterUsnJournal()
{
	BOOL ret = FALSE;
	FILE* fp;

	FileChangeRecordFilter recordFilter;
	recordFilter.Init(rootdir);
	
	std::deque<ORG_USN_RECORD> con;
	ret = EnumUsnRecord(con);

	
	setlocale(LC_CTYPE, "chs");
	for (std::deque<ORG_USN_RECORD>::const_iterator itor = con.begin(); itor != con.end(); ++itor)
	{
		const ORG_USN_RECORD& mur = *itor;
		wstring strPath = StringtoWstring(m_drive) + L":";

		GetFullPath(mur, con, strPath);


		PFileChangeRecord record = new FileChangeRecord();
		if (record == NULL)
		{
			LOG_ERROR << "memory alloc for record failed\n";
			return FALSE;
		}
		memset(record, 0, sizeof(FileChangeRecord));

		record->action = mur.Reason;
		record->TimeStamp = mur.TimeStamp;
		record->filename = WStringToString(wstring(mur.FileName));
		record->FileAttributes = mur.FileAttributes;
		record->dir = (mur.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
		record->currentPath = WStringToString(strPath);

		recordFilter.addRecord(record);
	}

	recordFilter.Filter();

	recordFilter.serialize(m_usnlogpath);

	return TRUE;
}

BOOL NtfsUsnJournal::GetFullPath(const ORG_USN_RECORD& mur, const deque<ORG_USN_RECORD>& con, wstring& Path)
{
	BOOL ret = FALSE;
	if ((mur.FileReferenceNumber & 0x0000FFFFFFFFFFFF) == 5)
		return true;

	deque<ORG_USN_RECORD>::const_iterator recent = con.end();
	for (deque<ORG_USN_RECORD>::const_iterator itor = con.begin(); itor != con.end() && itor->TimeStamp.QuadPart <= mur.TimeStamp.QuadPart; ++itor)
	{
		if (itor->FileReferenceNumber == mur.ParentFileReferenceNumber)
			recent = itor;
	}
	if (recent != con.end())
	{
		ret = GetFullPath(*recent, con, Path);
		Path += L"\\";
		Path += mur.FileName;
		return ret;
	}

	ret = GetFullPathByFileReferenceNumber(m_hVol, mur.ParentFileReferenceNumber, Path);
	Path += L"\\";
	Path += mur.FileName;
	return ret;
}

BOOL NtfsUsnJournal::GetFullPathByFileReferenceNumber(HANDLE hVol, DWORDLONG FileReferenceNumber, wstring& Path)
{
	UNICODE_STRING fidstr = { 8, 8, (PWSTR)&FileReferenceNumber };

	const ULONG OBJ_CASE_INSENSITIVE = 0x00000040UL;
	OBJECT_ATTRIBUTES oa = { sizeof(OBJECT_ATTRIBUTES), hVol, &fidstr, OBJ_CASE_INSENSITIVE, 0, 0 };

	HANDLE hFile;
	ULONG iosb[2];
	const ULONG FILE_OPEN_BY_FILE_ID = 0x00002000UL;
	const ULONG FILE_OPEN = 0x00000001UL;
	ULONG status = NtCreatefile(&hFile, FILE_READ_ATTRIBUTES | FILE_READ_EA, &oa, iosb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN, FILE_OPEN_BY_FILE_ID, NULL, 0);
	if (status == 0)
	{
		IO_STATUS_BLOCK IoStatus;
		size_t allocSize = sizeof(OBJECT_NAME_INFORMATION)+MAX_PATH*sizeof(WCHAR);
		POBJECT_NAME_INFORMATION pfni = (POBJECT_NAME_INFORMATION)operator new(allocSize);
		status = NtQueryInformationFile(hFile, &IoStatus, pfni, allocSize, FileNameInformation);
		if (status == 0)
		{
			if (pfni->Name.Length > 2)
			{
				Path.append((WCHAR*)&pfni->Name.Buffer, 0, pfni->Name.Length / 2);
			}
		}
		operator delete(pfni);

		CloseHandle(hFile);
	}
	return status == 0;
}

void NtfsUsnJournal::SetRootDir(const string& dir)
{
	rootdir = dir;
}