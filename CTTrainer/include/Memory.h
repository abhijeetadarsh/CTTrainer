#pragma once
// ============================================================
//  Memory.h  –  Process attach, pointer resolution, R/W
//  Part of CTTrainer — generic Cheat Engine CT-driven trainer
// ============================================================
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>

namespace Memory
{
    // Find PID by process name (wide)
    DWORD       GetProcessID(const std::wstring& processName);

    // Get base address of a module inside a process
    uintptr_t   GetModuleBase(DWORD pid, const std::wstring& moduleName);

    // Open process with PROCESS_ALL_ACCESS
    HANDLE      OpenTarget(DWORD pid);

    // Resolve a pointer chain: reads through each offset until the last.
    // is32Bit=true  → reads 4-byte (DWORD) pointers (32-bit target process)
    // is32Bit=false → reads 8-byte (QWORD) pointers (64-bit target process)
    // Returns 0 on any failure.
    uintptr_t   ResolveChain(HANDLE hProcess,
        uintptr_t baseAddress,
        const std::vector<uintptr_t>& offsets,
        bool is32Bit = true);

    // Typed read / write
    template<typename T>
    bool Read(HANDLE hProcess, uintptr_t address, T& out)
    {
        return ReadProcessMemory(hProcess, (LPCVOID)address,
            &out, sizeof(T), nullptr) != 0;
    }

    template<typename T>
    bool Write(HANDLE hProcess, uintptr_t address, const T& value)
    {
        return WriteProcessMemory(hProcess, (LPVOID)address,
            &value, sizeof(T), nullptr) != 0;
    }

    // Parse "0x1A2B" or "12345" → uintptr_t
    uintptr_t   ParseHex(const std::string& s);

    // Split "0x10, 0x4C, 0x8" → vector
    std::vector<uintptr_t> ParseOffsets(const std::string& s);
}