#pragma once
#include <string>
#include <Windows.h>
using namespace std;

enum tagFileActionType{
	FILE_CREATE = 1,
	FILE_MOVE = 2,
	FILE_MODIFY = 3,
	FILE_RENAMED = 4,
	FILE_DELETE = 5
};

typedef struct tagFileChangeRecord{
	LARGE_INTEGER TimeStamp;
	DWORD FileAttributes;
	WORD   FileNameLength;
	string filename;
	string currentPath;
	string originalPath;
	size_t fileSize;
	DWORD action;
	BOOL dir;
	struct tagFileChangeRecord* next;
	struct tagFileChangeRecord* prev;
} FileChangeRecord;
typedef FileChangeRecord* PFileChangeRecord;

class FileChangeRecordFilter {

public:
	FileChangeRecordFilter();
	~FileChangeRecordFilter();
	void Init(const string& rootdir);
	void Filter();
	void MergeRenameAction();
	void FilterModifyAction();
	void addRecord(const PFileChangeRecord& pfileRecord);
	void deleteRecord(PFileChangeRecord pfileRecord);
	string toJson(PFileChangeRecord pFileInfo);
	void serialize(string filename);
	void SetRemoteRootdir(const string& rootdir);

private:
	void MoveRecord(const PFileChangeRecord& pfileRecord, PFileChangeRecord& tgtListLast);
	PFileChangeRecord findRecord(const PFileChangeRecord& pfileRecord, PFileChangeRecord& tgtListHead);
	DWORD GetFileSize(const PFileChangeRecord& pfileRecord);
	template <class ValueType>
	void swap(ValueType& A, ValueType& B)
	{
		ValueType tmp = A;
		A = B;
		B = tmp;
	}
private:
	PFileChangeRecord m_Header;
	PFileChangeRecord m_Last;
	string remoteRootdir;

};