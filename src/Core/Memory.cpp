#include "../../include/Core/Memory.hpp"

namespace Core {
    bool Memory::Attach(const wchar_t* processName) {
        m_ProcessId = GetProcessId(processName);
        if (m_ProcessId == 0) return false;

        m_ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_ProcessId);
        return m_ProcessHandle != nullptr;
    }

    uintptr_t Memory::GetModuleBase(const wchar_t* moduleName) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_ProcessId);
        if (snapshot == INVALID_HANDLE_VALUE) return 0;

        MODULEENTRY32W moduleEntry;
        moduleEntry.dwSize = sizeof(moduleEntry);

        uintptr_t baseAddress = 0;
        if (Module32FirstW(snapshot, &moduleEntry)) {
            do {
                if (!_wcsicmp(moduleEntry.szModule, moduleName)) {
                    baseAddress = (uintptr_t)moduleEntry.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snapshot, &moduleEntry));
        }
        CloseHandle(snapshot);
        return baseAddress;
    }

    DWORD Memory::GetProcessId(const wchar_t* processName) {
        DWORD processId = 0;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W processEntry;
            processEntry.dwSize = sizeof(processEntry);
            if (Process32FirstW(snapshot, &processEntry)) {
                do {
                    if (!_wcsicmp(processEntry.szExeFile, processName)) {
                        processId = processEntry.th32ProcessID;
                        break;
                    }
                } while (Process32NextW(snapshot, &processEntry));
            }
            CloseHandle(snapshot);
        }
        return processId;
    }
}
