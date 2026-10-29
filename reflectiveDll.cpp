// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <windows.h>
#include <winternl.h>
#pragma function(wcslen)
#pragma function(memcpy)
#pragma function(strcmp)
typedef struct _LDR_DATA_TABLE_ENTRY_FULL { //doi ten _LDR_DATA_TABLE_ENTRY -> _LDR_DATA_TABLE_ENTRY_FULL de tranh xung dot voi dinh nghia trong winternl.h
    PVOID Reserved1[2];
    LIST_ENTRY InMemoryOrderLinks;
    PVOID Reserved2[2];
    PVOID DllBase;
    PVOID Reserved3[2];
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    PVOID Reserved5[3];
#pragma warning(push)
#pragma warning(disable: 4201) // we'll always use the Microsoft compiler
    union {
        ULONG CheckSum;
        PVOID Reserved6;
    } DUMMYUNIONNAME;
#pragma warning(pop)
    ULONG TimeDateStamp;
} LDR_DATA_TABLE_ENTRY_FULL, * PLDR_DATA_TABLE_ENTRY_FULL;
// ✅ ĐÚNG — có 2 tham số
typedef FARPROC(WINAPI* GetProcAddress_t)(HMODULE, LPCSTR);
typedef LPVOID(WINAPI* VirtualAlloc_t)(
    LPVOID lpAddress,
    SIZE_T dwSize,
    DWORD  flAllocationType,
    DWORD  flProtect
    );
typedef BOOL(WINAPI* WriteProcessMemory_t)(
    HANDLE  hProcess,
    LPVOID  lpBaseAddress,
    LPCVOID lpBuffer,
    SIZE_T  nSize,
    SIZE_T* lpNumberOfBytesWritten
    );
typedef HMODULE(WINAPI* LoadLibraryA_t)(
    LPCSTR lpLibFileName
    );
typedef BOOL(WINAPI* VirtialProtect_t)(
    LPVOID lpAddress,
    SIZE_T dwSize,
    DWORD  flNewProtect,
    PDWORD lpflOldProtect
    );
typedef BOOL(WINAPI* DllMain_t)(
    HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
    );
typedef int(WINAPI* MessageBoxW_t)(
    HWND    hWnd,
    LPCWSTR lpText,
    LPCWSTR lpCaption,
    UINT    uType
    );
size_t wcslen(LPCWSTR str) {
    size_t len = 0;
    while (str[len] != L'\0') len++;
    return len;
}
WCHAR towlower(WCHAR c) {
    if (c >= L'A' && c <= L'Z') {
        return c + (L'a' - L'A');  // 'A' → 'a'
    }
    return c;
}
BOOL UnicodeEqualsW(UNICODE_STRING ustr, const wchar_t* wide) {
    // 1. Kiểm tra độ dài
    if (ustr.Length == 0) return FALSE;
    if (ustr.Length / 2 != wcslen(wide)) return FALSE;

    // 2. So sánh từng wchar_t
    for (int i = 0; i < ustr.Length / 2; i++) {
        if (towlower(ustr.Buffer[i]) != towlower(wide[i]))
            return FALSE;
    }
    return TRUE;
}
LPVOID memcpy(LPVOID dest, LPCVOID src, size_t size) {
    BYTE* d = (BYTE*)dest;
    const BYTE* s = (const BYTE*)src;

    while (size--) {
        *d++ = *s++;
    }
    return dest;
}
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
VOID ExecuteShellcode() {
    MessageBoxW(NULL, L"injected", L"1122334", MB_OK);
}
extern "C"  __declspec(dllexport) VOID ReflectiveDllLoader() {
    // Trình biên dịch sẽ sinh mã: mov word ptr [rsp+...], 'k', mov word ptr [rsp+...], 'e', ...
    WCHAR kernel32dllWStr[] = { L'k', L'e', L'r', L'n', L'e', L'l', L'3', L'2', L'.', L'd', L'l', L'l', L'\0' };
    WCHAR user32dllWStr[] = { L'u', L's', L'e', L'r', L'3', L'2', L'.', L'd', L'l', L'l', L'\0' };

    CHAR GetProcAddressStr[] = { 'G', 'e', 't', 'P', 'r', 'o', 'c', 'A', 'd', 'd', 'r', 'e', 's', 's', '\0' };
    CHAR MessageBoxWStr[] = { 'M', 'e', 's', 's', 'a', 'g', 'e', 'B', 'o', 'x', 'W', '\0' };
    CHAR VirtualAllocStr[] = { 'V', 'i', 'r', 't', 'u', 'a', 'l', 'A', 'l', 'l', 'o', 'c', '\0' };
    CHAR LoadLibraryAStr[] = { 'L', 'o', 'a', 'd', 'L', 'i', 'b', 'r', 'a', 'r', 'y', 'A', '\0' };
    CHAR VirtualProtectStr[] = { 'V', 'i', 'r', 't', 'u', 'a', 'l', 'P', 'r', 'o', 't', 'e', 'c', 't', '\0' };
    WCHAR captionWStr[] = { L'i', L'\0' };
    WCHAR textWStr[] = { L'1', L'\0' };


    //tim base address
    ULONG_PTR baseaddr = (ULONG_PTR)&ReflectiveDllLoader;
    baseaddr = baseaddr & ~0xFFF;  // align page

    while (baseaddr > 0) {
        if (*(USHORT*)baseaddr == 0x5A4D) {  // 'MZ'
            DWORD e_lfanew = *(DWORD*)(baseaddr + 0x3C);
            if (*(DWORD*)(baseaddr + e_lfanew) == 0x00004550) {  // 'PE\0\0'
                break;
            }
        }
        baseaddr -= 0x1000;
    }
    if (baseaddr < 0) return;
    //while (1);
    //tim dia chi cac ham can dung VirtualAllocEx, WriteProcessMemory, 
    PPEB peb = (PPEB)__readgsqword(0x60); 
    PPEB_LDR_DATA ldr = peb->Ldr;
    LIST_ENTRY* head = &ldr->InMemoryOrderModuleList;
    LIST_ENTRY* entry = head->Flink;
    HMODULE hKernel32 = NULL;
    HMODULE hUser32 = NULL;
    PLDR_DATA_TABLE_ENTRY_FULL mod;
    //while (1);
    while (entry != head) {
        mod = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY_FULL, InMemoryOrderLinks);
        if (UnicodeEqualsW(mod->BaseDllName, kernel32dllWStr)) {
            hKernel32 = (HMODULE)mod->DllBase;
        }
        if (UnicodeEqualsW(mod->BaseDllName, user32dllWStr)) {
            hUser32 = (HMODULE)mod->DllBase;
        }
        entry = entry->Flink;
    }
    //while (1);
    if (hKernel32 == NULL || hUser32 == NULL)
        return;
    //while (1);
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hKernel32;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((LPBYTE)hKernel32 + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY expDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)((LPBYTE)hKernel32 + expDir.VirtualAddress);
    DWORD* names = (DWORD*)((LPBYTE)hKernel32 + exports->AddressOfNames);
    WORD* ordinals = (WORD*)((LPBYTE)hKernel32 + exports->AddressOfNameOrdinals);
    DWORD* functions = (DWORD*)((LPBYTE)hKernel32 + exports->AddressOfFunctions);
    WORD ordinal;
    DWORD rva;
    for (DWORD i = 0; i < exports->NumberOfNames; i++) {
        const char* name = (const char*)((LPBYTE)hKernel32 + names[i]);
        if (strcmp(name, GetProcAddressStr) == 0) {
            // Tìm thấy!
            ordinal = ordinals[i];
            rva = functions[ordinal];
            break;
        }
    }
    GetProcAddress_t pGetProcAddress = (GetProcAddress_t)((LPBYTE)hKernel32 + rva);
    if (pGetProcAddress == NULL)
        return;
    MessageBoxW_t pMessageBoxW = (MessageBoxW_t)pGetProcAddress(hUser32, MessageBoxWStr); //debug
    if (pMessageBoxW == NULL)
      return;
    // Sau khi có GetProcAddress
    VirtualAlloc_t pVirtualAlloc = (VirtualAlloc_t)pGetProcAddress(hKernel32, VirtualAllocStr);
  
    //WriteProcessMemory_t pWriteProcessMemory = (WriteProcessMemory_t)pGetProcAddress(hKernel32, "WriteProcessMemory");
    LoadLibraryA_t pLoadLibraryA = (LoadLibraryA_t)pGetProcAddress(hKernel32, LoadLibraryAStr);
    VirtialProtect_t pVirtualProtect = (VirtialProtect_t)pGetProcAddress(hKernel32, VirtualProtectStr);

    //cap phat vung nho trong tien trinh dang chay
    // Đọc PE header

    dos = (PIMAGE_DOS_HEADER)baseaddr;
    nt = (PIMAGE_NT_HEADERS)(baseaddr + dos->e_lfanew);
    // Lấy SizeOfImage
    DWORD sizeOfImage = nt->OptionalHeader.SizeOfImage;
    //cap phat region2
    LPVOID region2 = pVirtualAlloc(
        NULL,                       // để Windows chọn địa chỉ
        sizeOfImage,            // kích thước DLL thô
        MEM_COMMIT | MEM_RESERVE,   // commit + reserve
        PAGE_READWRITE      // RW (cần thiết để chạy loader)
    );

    if (!region2) {
        return;
    }

    //copy headers
    memcpy(
        region2,                                    // đích: Vùng 2
        (LPVOID)baseaddr,                             // nguồn: Vùng 1
        nt->OptionalHeader.SizeOfHeaders          // số bytes
    );

    //copy sections
    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);

    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {

        // Giới hạn bởi VirtualSize nếu cần
        if (sec->SizeOfRawData > sec->Misc.VirtualSize) {
            memcpy(
                (LPBYTE)region2 + sec->VirtualAddress,
                (LPBYTE)baseaddr + sec->PointerToRawData,
                sec->Misc.VirtualSize
            );
        }
        else {
            memcpy(
                (LPBYTE)region2 + sec->VirtualAddress,
                (LPBYTE)baseaddr + sec->PointerToRawData,
                sec->SizeOfRawData
            );
        }
    }

    //resolve imports

    PIMAGE_IMPORT_DESCRIPTOR imp = (PIMAGE_IMPORT_DESCRIPTOR)((LPBYTE)region2 + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    char* dllName;
    HMODULE hDep;
    PIMAGE_THUNK_DATA origThunk;
    PIMAGE_THUNK_DATA firstThunk;
    FARPROC func;
    PIMAGE_IMPORT_BY_NAME ibn;
    while (imp->Name != 0) {
        dllName = (char*)((LPBYTE)region2 + imp->Name);

        // Load DLL phụ thuộc
        hDep = pLoadLibraryA(dllName); //fix
        if (hDep == NULL)
            return;
        // Resolve từng hàm
        origThunk = (PIMAGE_THUNK_DATA)((LPBYTE)region2 + imp->OriginalFirstThunk);
        firstThunk = (PIMAGE_THUNK_DATA)((LPBYTE)region2 + imp->FirstThunk);
        while (origThunk->u1.AddressOfData != 0) {

            if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) {
                // Import theo ordinal
                func = pGetProcAddress(hDep, (LPCSTR)IMAGE_ORDINAL(origThunk->u1.Ordinal));
            }
            else {
                // Import theo tên
                ibn = (PIMAGE_IMPORT_BY_NAME)(
                    (LPBYTE)region2 + origThunk->u1.AddressOfData
                    );
                func = pGetProcAddress(hDep, ibn->Name);
            }

            // Ghi địa chỉ thật vào IAT
            firstThunk->u1.Function = (ULONG_PTR)func;
            origThunk++;
            firstThunk++;
        }
        imp++;
    }

    //apply relocations
    ULONG_PTR delta = (ULONG_PTR)region2- nt->OptionalHeader.ImageBase;

    if (delta != 0 && nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size > 0) {

        PIMAGE_BASE_RELOCATION reloc = (PIMAGE_BASE_RELOCATION)(
            (LPBYTE)region2 + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress
            );
        DWORD count;
        WORD* entries;
        while (reloc->VirtualAddress != 0) {
            count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            entries = (WORD*)(reloc + 1);

            for (DWORD i = 0; i < count; i++) {
                WORD type = entries[i] >> 12;
                WORD offset = entries[i] & 0xFFF;

                if (type == IMAGE_REL_BASED_HIGHLOW) {       // 32-bit Relocation (Luôn luôn là 4 bytes)
                    DWORD* ptr = (DWORD*)((LPBYTE)region2 + reloc->VirtualAddress + offset);
                    *ptr += (DWORD)delta;
                }
                else if (type == IMAGE_REL_BASED_DIR64) {    // 64-bit
                    ULONGLONG* ptr = (ULONGLONG*)((LPBYTE)region2 + reloc->VirtualAddress + offset);
                    *ptr += delta;
                }
            }
            reloc = (PIMAGE_BASE_RELOCATION)((LPBYTE)reloc + reloc->SizeOfBlock);
        }
    }

    //set protection cho sections
    sec = IMAGE_FIRST_SECTION(nt);
    DWORD old;
    DWORD prot;
    DWORD characteristics;
    BOOL hasExecute;
    BOOL hasRead;
    BOOL hasWrite;
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        prot = 0;
        characteristics = sec->Characteristics;

        hasExecute = (characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        hasRead = (characteristics & IMAGE_SCN_MEM_READ) != 0;
        hasWrite = (characteristics & IMAGE_SCN_MEM_WRITE) != 0;

        // Chọn giá trị PAGE_* phù hợp
        if (hasExecute && hasRead && hasWrite) {
            prot = PAGE_EXECUTE_READWRITE;  // 0x40
        }
        else if (hasExecute && hasRead) {
            prot = PAGE_EXECUTE_READ;       // 0x20
        }
        else if (hasExecute) {
            prot = PAGE_EXECUTE;            // 0x10
        }
        else if (hasRead && hasWrite) {
            prot = PAGE_READWRITE;          // 0x04
        }
        else if (hasRead) {
            prot = PAGE_READONLY;           // 0x02
        }
        else {
            prot = PAGE_NOACCESS;           // 0x01
        }

        pVirtualProtect((LPBYTE)region2 + sec->VirtualAddress,
            sec->Misc.VirtualSize,
            prot, &old);
    }

    //set protection cho headers
    pVirtualProtect(
        region2,                                    // ← base address
        nt->OptionalHeader.SizeOfHeaders,          // ← kích thước headers
        PAGE_READONLY,                             // ← READONLY
        &old
    );

    //goi DllMain

    // Lấy AddressOfEntryPoint từ PE header
    DWORD entryRVA = nt->OptionalHeader.AddressOfEntryPoint;

    // Tính địa chỉ thật
    DllMain_t pDllMain = (DllMain_t)((LPBYTE)region2 + entryRVA);
    // Gọi DllMain với DLL_PROCESS_ATTACH
    pDllMain((HINSTANCE)region2, DLL_PROCESS_ATTACH, NULL);
}
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        ExecuteShellcode();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

