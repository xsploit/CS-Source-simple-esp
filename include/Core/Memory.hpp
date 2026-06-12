#pragma once
#include <windows.h>
#include <iostream>
#include <vector>
#include <tlhelp32.h>

namespace Core {
    class Memory {
    public:
        Memory() = default;
        ~Memory() {
            if (m_ProcessHandle)
                CloseHandle(m_ProcessHandle);
        }

        bool Attach(const wchar_t* processName);
        uintptr_t GetModuleBase(const wchar_t* moduleName);

        template <typename T>
        T Read(uintptr_t address) {
            T value = {};
            ReadProcessMemory(m_ProcessHandle, (LPCVOID)address, &value, sizeof(T), nullptr);
            return value;
        }

        template <typename T>
        bool Read(uintptr_t address, T& out) {
            return ReadProcessMemory(m_ProcessHandle, (LPCVOID)address, &out, sizeof(T), nullptr);
        }

        template <typename T>
        bool Write(uintptr_t address, const T& value) {
            return WriteProcessMemory(m_ProcessHandle, (LPVOID)address, &value, sizeof(T), nullptr);
        }

        HANDLE GetHandle() const { return m_ProcessHandle; }
        DWORD GetId() const { return m_ProcessId; }
        DWORD GetProcessId(const wchar_t* processName);

    private:
        HANDLE m_ProcessHandle = nullptr;
        DWORD m_ProcessId = 0;
    };
}
