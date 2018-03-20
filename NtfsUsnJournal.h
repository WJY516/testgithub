#include <windows.h>
#include <string>
#include <deque>
#include <set>
using namespace std;

typedef ULONG(__stdcall *PNtCreateFile)(
	PHANDLE FileHandle,
	ULONG DesiredAccess,
	PVOID ObjectAttributes,
	PVOID IoStatusBlock,
	PLARGE_INTEGER AllocationSize,
	ULONG FileAttributes,
	ULONG ShareAccess,
	ULONG CreateDisposition,
	ULONG CreateOptions,
	PVOID EaBuffer,
	ULONG EaLength);

typedef struct _UNICODE_STRING {
	USHORT Length, MaximumLength;
	PWCH Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
	ULONG Length;
	HANDLE RootDirectory;
	PUNICODE_STRING ObjectName;
	ULONG Attributes;
	PVOID SecurityDescriptor;
	PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES;

typedef struct _IO_STATUS_BLOCK {
	union {
		NTSTATUS Status;
		PVOID Pointer;
	};
	ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;
typedef enum _FILE_INFORMATION_CLASS {
	// бнбн
	FileNameInformation = 9
	// бнбн
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;
typedef NTSTATUS(__stdcall *PNtQueryInformationFile)(
	HANDLE FileHandle,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID FileInformation,
	DWORD Length,
	FILE_INFORMATION_CLASS FileInformationClass);

typedef struct _OBJECT_NAME_INFORMATION {
	UNICODE_STRING Name;
} OBJECT_NAME_INFORMATION, *POBJECT_NAME_INFORMATION;

struct ORG_USN_RECORD
{
	DWORDLONG FileReferenceNumber;
	DWORDLONG ParentFileReferenceNumber;
	LARGE_INTEGER TimeStamp;
	DWORD Reason;
	DWORD FileAttributes;
	WORD   FileNameLength;
	WCHAR FileName[MAX_PATH];
};

class NtfsUsnJournal {
public:
	NtfsUsnJournal();
	~NtfsUsnJournal();
	BOOL CreateUsnJournal(string drvname);
	BOOL DeleteUsnJournal();
	BOOL FilterUsnJournal();
	void SetRootDir(const string& dir);
private:
	void EnablePrivilege();
	BOOL EnumUsnRecord(std::deque<ORG_USN_RECORD>& con);
	BOOL GetFullPath(const ORG_USN_RECORD& mur, const deque<ORG_USN_RECORD>& con, wstring& Path);
	BOOL GetFullPathByFileReferenceNumber(HANDLE hVol, DWORDLONG FileReferenceNumber, wstring& Path);

private:
	HANDLE m_hVol;
	USN_JOURNAL_DATA m_qujd;
	string m_drive;
	string rootdir;
	string m_usnlogpath;

	PNtCreateFile NtCreatefile;
	PNtQueryInformationFile NtQueryInformationFile;
};