#include <string>
#include "EventFilter.h"
#include <time.h>
#include <fstream>
#include "json.h"
using namespace std;

#define PAGE_SIZE 4096
#define FIBONACCI_MULTIPLIER 2654435769
#define FIBONACCI_HASH(x)  ((x*FIBONACCI_MULTIPLIER)>>22)


string FileName(string sFile)
{
	//    wstring Fname;
	string::size_type last_pos;
	last_pos = sFile.find_last_of("\\");
	if (last_pos == wstring::npos)        // sFile is the filename already
		return sFile;
	else                            // Get rid of path for filename
	{
		sFile.erase(0, last_pos + 1);
		return sFile;

	}
}

void replace_all(std::string & s, std::string const & t, std::string const & w)
{
	if (s.empty())
		return;
	string::size_type pos = s.find(t), t_size = t.size(), r_size = w.size();
	while (pos != std::string::npos){ // found   
		s.replace(pos, t_size, w);
		pos = s.find(t, pos + r_size);
	}
}


BOOL IsDir(const string& Path)
{
	DWORD dwAttrs = GetFileAttributesA(Path.c_str());
	DWORD err = GetLastError();
	return (dwAttrs & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
}

time_t GetCurTime()
{
	LARGE_INTEGER liTime;
	LARGE_INTEGER liFrequency;

	liTime.QuadPart = 0;
	liFrequency.QuadPart = 1;

	::QueryPerformanceCounter(&liTime);
	::QueryPerformanceFrequency(&liFrequency);

	return (time_t)(liTime.QuadPart * 1000 / liFrequency.QuadPart);
}

DWORD FileChangeRecordFilter::GetFileSize(const PFileChangeRecord& pfileRecord)
{
	WIN32_FIND_DATAA ffd;


	if (pfileRecord->dir)
		return 0;

	DWORD size = 0;
	HANDLE hFind = FindFirstFileA(pfileRecord->currentPath.c_str(), &ffd);
	if (INVALID_HANDLE_VALUE == hFind)
	{
		return 0;
	}
	size = ffd.nFileSizeLow;
	FindClose(hFind);

	return size;
}

FileChangeRecordFilter::FileChangeRecordFilter() :m_Header(NULL), m_Last(NULL)
{

}

FileChangeRecordFilter::~FileChangeRecordFilter()
{

}


void FileChangeRecordFilter::Init(const string& rootdir)
{
	remoteRootdir = rootdir;
	m_Header = (PFileChangeRecord)malloc(sizeof(FileChangeRecord));
	memset(m_Header, 0, sizeof(FileChangeRecord));
	m_Last = m_Header;
	/*TODO:init hash map*/
}

void FileChangeRecordFilter::SetRemoteRootdir(const string& rootdir)
{
	remoteRootdir = rootdir;
}

void FileChangeRecordFilter::addRecord(const PFileChangeRecord& pfileRecord)
{
	pfileRecord->prev = m_Last;
	pfileRecord->next = NULL;
	m_Last->next = pfileRecord;
	m_Last = pfileRecord;
}

void FileChangeRecordFilter::MoveRecord(const PFileChangeRecord& pfileRecord, PFileChangeRecord& tgtListLast)
{
	/*remove from current list*/
	if (!pfileRecord)
		return;
	if (m_Last == pfileRecord)
	{
		m_Last = pfileRecord->prev;
		m_Last->next = NULL;
	}
	else
	{
		pfileRecord->prev->next = pfileRecord->next;
		pfileRecord->next->prev = pfileRecord->prev;
	}

	/*move to target list*/
	pfileRecord->prev = tgtListLast;
	pfileRecord->next = NULL;
	tgtListLast->next = pfileRecord;
	tgtListLast = pfileRecord;
}

void FileChangeRecordFilter::deleteRecord(PFileChangeRecord pfileRecord)
{
	if (!pfileRecord)
		return;
	if (m_Last == pfileRecord)
	{
		m_Last = pfileRecord->prev;
		m_Last->next = NULL;
	}
	else
	{
		pfileRecord->prev->next = pfileRecord->next;
		pfileRecord->next->prev = pfileRecord->prev;
	}
	delete pfileRecord;
}

PFileChangeRecord FileChangeRecordFilter::findRecord(const PFileChangeRecord& pfileRecord, PFileChangeRecord& tgtListHead)
{

	PFileChangeRecord next = tgtListHead->next;
	while (next)
	{
		if (!next->currentPath.compare(pfileRecord->currentPath))
			return next;
		next = next->next;
	}
	return NULL;

}

void FileChangeRecordFilter::Filter()
{
	MergeRenameAction();
	FilterModifyAction();
}

void FileChangeRecordFilter::MergeRenameAction()
{
	PFileChangeRecord renameoldRecord = NULL;
	PFileChangeRecord record = m_Header->next;
	while (record)
	{
		DWORD xxxx = record->action & (USN_REASON_CLOSE | USN_REASON_RENAME_NEW_NAME);
		DWORD yyyy = record->action & (USN_REASON_CLOSE | USN_REASON_RENAME_OLD_NAME);
		if ((record->action & (USN_REASON_CLOSE | USN_REASON_RENAME_NEW_NAME)) == (USN_REASON_CLOSE | USN_REASON_RENAME_NEW_NAME))
		{
			PFileChangeRecord oldRecord = record;
			record = record->next;
			deleteRecord(oldRecord);
			continue;
		}
		else if ((record->action & (USN_REASON_CLOSE | USN_REASON_RENAME_OLD_NAME)) == (USN_REASON_CLOSE | USN_REASON_RENAME_OLD_NAME))
		{
			PFileChangeRecord oldRecord = record;
			record = record->next;
			deleteRecord(oldRecord);
			continue;
		}
		else if (record->action & USN_REASON_RENAME_OLD_NAME)
			renameoldRecord = record;
		else if (record->action & USN_REASON_RENAME_NEW_NAME)
		{
			record->originalPath = renameoldRecord->currentPath;
			deleteRecord(renameoldRecord);
			renameoldRecord = NULL;
		}
		else
		{
			record = record->next;
			continue;
		}
		record = record->next;

	}
}

void FileChangeRecordFilter::FilterModifyAction()
{
	PFileChangeRecord HeaderBak = (PFileChangeRecord)malloc(sizeof(FileChangeRecord));
	memset(HeaderBak, 0, sizeof(FileChangeRecord));
	PFileChangeRecord LastBak = HeaderBak;

	PFileChangeRecord record = m_Header->next;
	while (record)
	{
		PFileChangeRecord oldRecord = NULL;
		size_t filePathLength = record->currentPath.size();
		string curPath = record->currentPath;
		DWORD action = record->action;
		if (action & USN_REASON_FILE_DELETE) {
			record->action = FILE_DELETE;

			BOOL prevfind = FALSE;
			PFileChangeRecord prevRecord = HeaderBak->next;
			while (prevRecord)
			{
				if (!prevRecord->currentPath.compare(record->currentPath))
				{
					prevfind = TRUE;
					break;
				}
				prevRecord = prevRecord->next;
			}
			if (!prevfind) {
				oldRecord = record;
				record = record->next;
				MoveRecord(oldRecord, LastBak);
			}
			else
			{
				if (prevRecord->action == FILE_CREATE && !prevRecord->dir)
				{
					if (LastBak == prevRecord)
					{
						LastBak = prevRecord->prev;
						LastBak->next = NULL;
					}
					else
					{
						prevRecord->prev->next = prevRecord->next;
						prevRecord->next->prev = prevRecord->prev;
					}
					delete prevRecord;

					record = record->next;
				}
				else if (prevRecord->action == FILE_MODIFY)
				{
					prevRecord->action = FILE_DELETE;
					record = record->next;
				}
				else
				{
					oldRecord = record;
					record = record->next;
					MoveRecord(oldRecord, LastBak);
				}
			}

		}
		else if (action &  USN_REASON_FILE_CREATE) {
			record->action = FILE_CREATE;

			BOOL prevfind = FALSE;
			PFileChangeRecord prevRecord = HeaderBak->next;
			while (prevRecord)
			{
				if (!prevRecord->currentPath.compare(record->currentPath))
				{
					prevfind = TRUE;
					break;
				}
				prevRecord = prevRecord->next;
			}
			if (!prevfind) {
				oldRecord = record;
				record = record->next;
				MoveRecord(oldRecord, LastBak);
			}
			else
			{
				oldRecord = record;
				record = record->next;
				deleteRecord(oldRecord);
			}
		}
		else if (action &  USN_REASON_RENAME_NEW_NAME) {
			record->action = record->currentPath.rfind("\\") == record->originalPath.rfind("\\") ? FILE_RENAMED : FILE_MOVE;

			BOOL prevfind = FALSE;
			PFileChangeRecord prevRecord = HeaderBak->next;
			while (prevRecord)
			{
				if (!prevRecord->currentPath.compare(record->originalPath))
				{
					prevfind = TRUE;
					break;
				}
				prevRecord = prevRecord->next;
			}
			if (!prevfind) {
				oldRecord = record;
				record = record->next;
				MoveRecord(oldRecord, LastBak);
			}
			else
			{
				if (prevRecord->action == FILE_CREATE && !prevRecord->dir)
				{
					if (LastBak == prevRecord)
					{
						LastBak = prevRecord->prev;
						LastBak->next = NULL;
					}
					else
					{
						prevRecord->prev->next = prevRecord->next;
						prevRecord->next->prev = prevRecord->prev;
					}
					delete prevRecord;

					record->action = FILE_CREATE;
					record->originalPath.clear();

					oldRecord = record;
					record = record->next;
					MoveRecord(oldRecord, LastBak);
				}
				else
				{
					oldRecord = record;
					record = record->next;
					MoveRecord(oldRecord, LastBak);
				}
			}
		}
		else {
			record->action = FILE_MODIFY;

			BOOL prevfind = FALSE;
			PFileChangeRecord prevRecord = HeaderBak->next;
			while (prevRecord)
			{
				if (!prevRecord->currentPath.compare(record->currentPath) && (prevRecord->action == FILE_CREATE || prevRecord->action == FILE_MODIFY))
				{
					prevfind = TRUE;
					break;
				}
				prevRecord = prevRecord->next;
			}
			if (!prevfind) {
				oldRecord = record;
				record = record->next;
				MoveRecord(oldRecord, LastBak);
			}
			else
			{
				oldRecord = record;
				record = record->next;
				deleteRecord(oldRecord);
			}
		}
	}

	swap(m_Header, HeaderBak);
	swap(m_Last, LastBak);
	delete HeaderBak;
}

string FileChangeRecordFilter::toJson(PFileChangeRecord pfileRecord)
{
	Json::Value var;


	if (pfileRecord->action == FILE_MODIFY ||
		pfileRecord->action == FILE_CREATE ||
		pfileRecord->action == FILE_RENAMED)
	{
		pfileRecord->dir = IsDir(pfileRecord->currentPath);
		pfileRecord->fileSize = GetFileSize(pfileRecord);
	}

	var["currentPath"] = pfileRecord->currentPath;
	var["originalPath"] = pfileRecord->originalPath;
	var["size"] = pfileRecord->fileSize;
	var["timestamp"] = pfileRecord->TimeStamp.QuadPart;
	var["type"] = (unsigned)pfileRecord->action;
	var["dir"] = pfileRecord->dir;

	Json::FastWriter writer;
	return writer.write(var);
}

void FileChangeRecordFilter::serialize(string filename)
{
	string json;
	ofstream file;
	Json::Value root;
	Json::FastWriter writer;

	file.open(filename);

	PFileChangeRecord next = m_Header->next;
	while (next)
	{

		Json::Value var;


		next->fileSize = GetFileSize(next);

		/****************for support linux fs ****************/
		next->currentPath.replace(0, 2, remoteRootdir);
		if (next->originalPath.size())
		{
			next->originalPath.replace(0, 2, remoteRootdir);
		}
		
		replace_all(next->currentPath, "\\", "/");
		replace_all(next->originalPath, "\\", "/");
		/********************************************/

		var["currentPath"] = next->currentPath;
		var["originalPath"] = next->originalPath;
		var["size"] = next->fileSize;
		var["timestamp"] = next->TimeStamp.QuadPart;
		var["type"] = (unsigned)next->action;
		var["dir"] = next->dir;

		root.append(var);

		next = next->next;
	}
	json = writer.write(root);
	file << json;
	file.flush();
}

 