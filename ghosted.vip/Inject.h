#include "lazy.hpp"
#include <TlHelp32.h>
#include <Psapi.h>


#pragma comment(lib, "Ws2_32.lib")
using namespace std;
#define MAX_PROCESSES 1024
typedef HMODULE(WINAPI* pLoadLibraryA)(LPCSTR);
typedef FARPROC(WINAPI* pGetProcAddress)(HMODULE, LPCSTR);
typedef BOOL(WINAPI* PDLL_MAIN)(HMODULE, DWORD, PVOID);

typedef struct _MANUAL_INJECT
{
    PVOID ImageBase;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_BASE_RELOCATION BaseRelocation;
    PIMAGE_IMPORT_DESCRIPTOR ImportDirectory;
    pLoadLibraryA fnLoadLibraryA;
    pGetProcAddress fnGetProcAddress;
} MANUAL_INJECT, * PMANUAL_INJECT;

DWORD WINAPI LoadDll(PVOID p)
{
    PMANUAL_INJECT ManualInject;

    HMODULE hModule;
    DWORD i, Function, count, delta;

    PDWORD ptr;
    PWORD list;

    PIMAGE_BASE_RELOCATION pIBR;
    PIMAGE_IMPORT_DESCRIPTOR pIID;
    PIMAGE_IMPORT_BY_NAME pIBN;
    PIMAGE_THUNK_DATA FirstThunk, OrigFirstThunk;

    PDLL_MAIN EntryPoint;

    ManualInject = (PMANUAL_INJECT)p;

    pIBR = ManualInject->BaseRelocation;
    delta = (DWORD)((LPBYTE)ManualInject->ImageBase - ManualInject->NtHeaders->OptionalHeader.ImageBase);

    while (pIBR->VirtualAddress)
    {
        if (pIBR->SizeOfBlock >= sizeof(IMAGE_BASE_RELOCATION))
        {
            count = (pIBR->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            list = (PWORD)(pIBR + 1);

            for (i = 0; i < count; i++)
            {
                if (list[i])
                {
                    ptr = (PDWORD)((LPBYTE)ManualInject->ImageBase + (pIBR->VirtualAddress + (list[i] & 0xFFF)));
                    *ptr += delta;
                }
            }
        }
        pIBR = (PIMAGE_BASE_RELOCATION)((LPBYTE)pIBR + pIBR->SizeOfBlock);
    }

    pIID = ManualInject->ImportDirectory;

    while (pIID->Characteristics)
    {
        OrigFirstThunk = (PIMAGE_THUNK_DATA)((LPBYTE)ManualInject->ImageBase + pIID->OriginalFirstThunk);
        FirstThunk = (PIMAGE_THUNK_DATA)((LPBYTE)ManualInject->ImageBase + pIID->FirstThunk);

        hModule = ManualInject->fnLoadLibraryA((LPCSTR)ManualInject->ImageBase + pIID->Name);

        if (!hModule)
            return FALSE;

        while (OrigFirstThunk->u1.AddressOfData)
        {
            if (OrigFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)
            {
                Function = (DWORD)ManualInject->fnGetProcAddress(hModule, (LPCSTR)(OrigFirstThunk->u1.Ordinal & 0xFFFF));

                if (!Function)
                    return FALSE;

                FirstThunk->u1.Function = Function;
            }
            else
            {
                pIBN = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)ManualInject->ImageBase + OrigFirstThunk->u1.AddressOfData);
                Function = (DWORD)ManualInject->fnGetProcAddress(hModule, (LPCSTR)pIBN->Name);

                if (!Function)
                    return FALSE;

                FirstThunk->u1.Function = Function;
            }

            OrigFirstThunk++;
            FirstThunk++;
        }

        pIID++;
    }

    if (ManualInject->NtHeaders->OptionalHeader.AddressOfEntryPoint)
    {
        EntryPoint = (PDLL_MAIN)((LPBYTE)ManualInject->ImageBase + ManualInject->NtHeaders->OptionalHeader.AddressOfEntryPoint);
        return EntryPoint((HMODULE)ManualInject->ImageBase, DLL_PROCESS_ATTACH, NULL);
    }

    return TRUE;
}

DWORD WINAPI LoadDllEnd()
{
    return 0;
}

DWORD ProcId = 0;

DWORD FindProcess(LPCTSTR lpcszFileName)
{
    LPDWORD lpdwProcessIds;
    LPTSTR lpszBaseName;
    HANDLE hProcess;
    DWORD i, cdwProcesses, dwProcessId = 0;

    lpdwProcessIds = (LPDWORD)HeapAlloc(GetProcessHeap(), 0, MAX_PROCESSES * sizeof(DWORD));
    if (lpdwProcessIds != NULL)
    {
        if (EnumProcesses(lpdwProcessIds, MAX_PROCESSES * sizeof(DWORD), &cdwProcesses))
        {
            lpszBaseName = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, MAX_PATH * sizeof(TCHAR));
            if (lpszBaseName != NULL)
            {
                cdwProcesses /= sizeof(DWORD);
                for (i = 0; i < cdwProcesses; i++)
                {
                    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, lpdwProcessIds[i]);
                    if (hProcess != NULL)
                    {
                        if (GetModuleBaseName(hProcess, NULL, lpszBaseName, MAX_PATH) > 0)
                        {
                            if (!lstrcmpi(lpszBaseName, lpcszFileName))
                            {
                                dwProcessId = lpdwProcessIds[i];
                                CloseHandle(hProcess);
                                break;
                            }
                        }
                        CloseHandle(hProcess);
                    }
                }
                HeapFree(GetProcessHeap(), 0, (LPVOID)lpszBaseName);
            }
        }
        HeapFree(GetProcessHeap(), 0, (LPVOID)lpdwProcessIds);
    }
    return dwProcessId;
}

DWORD MyGetProcessId(const char* pName)
{
    HANDLE pSnapshot = LI_FN(CreateToolhelp32Snapshot)(TH32CS_SNAPPROCESS, 0);

    if (!pSnapshot)
        return 0;

    PROCESSENTRY32 pInfo{ 0 };
    pInfo.dwSize = sizeof(PROCESSENTRY32);

    if (LI_FN(Process32First)(pSnapshot, &pInfo))
    {
        while (LI_FN(Process32Next)(pSnapshot, &pInfo))
        {
            if (strstr(pName, pInfo.szExeFile))
            {
                LI_FN(CloseHandle)(pSnapshot);

                return pInfo.th32ProcessID;
            }
        }
    }

    LI_FN(CloseHandle)(pSnapshot);

    return 0;
}

PIMAGE_DOS_HEADER pIDH;
PIMAGE_NT_HEADERS pINH;
PIMAGE_SECTION_HEADER pISH;
HANDLE hProcess, hThread, hFile, hToken;
PVOID buffer, image, mem;
DWORD i, FileSize, ProcessId, ExitCode;
TOKEN_PRIVILEGES tp;
MANUAL_INJECT ManualInject;

bool bypass = false;