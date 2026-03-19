// ============================================================
//  Memory.cpp  –  Part of CTTrainer
// ============================================================
#include "Memory.h"
#include "Logger.h"

#include <sstream>

// Helper: format uintptr_t as hex string
static std::string Hex(uintptr_t v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)v);
    return buf;
}

namespace Memory
{
    DWORD GetProcessID(const std::wstring& processName)
    {
        DWORD  pid = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
        {
            LOG_ERR("GetProcessID: CreateToolhelp32Snapshot failed");
            return 0;
        }

        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(snap, &pe))
            do {
                if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0)
                {
                    pid = pe.th32ProcessID; break;
                }
            } while (Process32NextW(snap, &pe));

        CloseHandle(snap);

        if (pid)
            LOG_INFO("GetProcessID: found PID " + std::to_string(pid));
        else
        {
            std::string n(processName.begin(), processName.end());
            LOG_ERR("GetProcessID: process not found: " + n);
        }
        return pid;
    }

    uintptr_t GetModuleBase(DWORD pid, const std::wstring& moduleName)
    {
        std::string mname(moduleName.begin(), moduleName.end());
        uintptr_t base = 0;

        HANDLE snap = CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            LOG_ERR("GetModuleBase: snapshot failed for '" + mname +
                "' GLE=" + std::to_string(err));
            return 0;
        }

        MODULEENTRY32W me = { sizeof(me) };
        if (Module32FirstW(snap, &me))
            do {
                if (_wcsicmp(me.szModule, moduleName.c_str()) == 0)
                {
                    base = (uintptr_t)me.modBaseAddr; break;
                }
            } while (Module32NextW(snap, &me));

        CloseHandle(snap);

        if (base)
            LOG_INFO("GetModuleBase: '" + mname + "' base = " + Hex(base));
        else
            LOG_ERR("GetModuleBase: module '" + mname + "' NOT FOUND in PID " +
                std::to_string(pid));

        return base;
    }

    HANDLE OpenTarget(DWORD pid)
    {
        HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (h)
            LOG_INFO("OpenTarget: handle opened for PID " + std::to_string(pid));
        else
            LOG_ERR("OpenTarget: OpenProcess failed PID=" + std::to_string(pid) +
                " GLE=" + std::to_string(GetLastError()));
        return h;
    }

    // ── Pointer chain resolution ─────────────────────────────
    //
    // Supports both 32-bit and 64-bit target processes:
    //   is32Bit=true  → reads DWORD (4 bytes) at each pointer step
    //   is32Bit=false → reads QWORD (8 bytes) at each pointer step
    //
    // CE traversal pattern:
    //   addr  = ReadPtr( moduleBase + staticOffset )
    //   addr  = ReadPtr( addr + offsets[0] )
    //   ...
    //   final =          addr + offsets[last]   ← no final deref
    //
    uintptr_t ResolveChain(HANDLE hProcess,
        uintptr_t baseAddress,
        const std::vector<uintptr_t>& offsets,
        bool is32Bit)
    {
        LOG_DEBUG("ResolveChain: static base = " + Hex(baseAddress) +
            "  offsets count = " + std::to_string(offsets.size()) +
            (is32Bit ? "  [32-bit]" : "  [64-bit]"));

        // Helper: read one pointer (4 or 8 bytes) from the target process
        auto readPtr = [&](uintptr_t addr, uintptr_t& out) -> bool
        {
            if (is32Bit)
            {
                DWORD v = 0;
                if (!ReadProcessMemory(hProcess, (LPCVOID)addr, &v, sizeof(v), nullptr) || v == 0)
                    return false;
                out = (uintptr_t)v;
            }
            else
            {
                uint64_t v = 0;
                if (!ReadProcessMemory(hProcess, (LPCVOID)addr, &v, sizeof(v), nullptr) || v == 0)
                    return false;
                out = (uintptr_t)v;
            }
            return true;
        };

        if (offsets.empty())
        {
            LOG_DEBUG("ResolveChain: no offsets, returning base directly");
            return baseAddress;
        }

        // Step 1: read the static pointer
        uintptr_t addr = 0;
        if (!readPtr(baseAddress, addr))
        {
            DWORD err = GetLastError();
            LOG_ERR("ResolveChain: FAILED deref static base " +
                Hex(baseAddress) + "  GLE=" + std::to_string(err));
            return 0;
        }
        LOG_DEBUG("ResolveChain: deref " + Hex(baseAddress) + " -> " + Hex(addr));

        // Step 2: walk all offsets except the last
        for (size_t i = 0; i + 1 < offsets.size(); ++i)
        {
            uintptr_t readFrom = addr + offsets[i];
            uintptr_t next = 0;
            if (!readPtr(readFrom, next))
            {
                DWORD err = GetLastError();
                LOG_ERR("ResolveChain: FAILED deref at step " +
                    std::to_string(i) + "  addr=" + Hex(readFrom) +
                    " (base=" + Hex(addr) + " + offset=" + Hex(offsets[i]) +
                    ")  GLE=" + std::to_string(err));
                return 0;
            }
            LOG_DEBUG("ResolveChain: step " + std::to_string(i) +
                "  deref " + Hex(readFrom) +
                " (+" + Hex(offsets[i]) + ") -> " + Hex(next));
            addr = next;
        }

        // Step 3: final offset = field address, NOT dereferenced
        uintptr_t finalAddr = addr + offsets.back();
        LOG_DEBUG("ResolveChain: final = " + Hex(addr) +
            " + " + Hex(offsets.back()) + " = " + Hex(finalAddr));
        LOG_INFO("ResolveChain: resolved to " + Hex(finalAddr));
        return finalAddr;
    }

    uintptr_t ParseHex(const std::string& s)
    {
        if (s.empty()) return 0;
        try { return (uintptr_t)std::stoull(s, nullptr, 0); }
        catch (...) { return 0; }
    }

    std::vector<uintptr_t> ParseOffsets(const std::string& s)
    {
        std::vector<uintptr_t> out;
        std::stringstream ss(s);
        std::string tok;
        while (std::getline(ss, tok, ','))
        {
            while (!tok.empty() && tok.front() == ' ') tok.erase(tok.begin());
            while (!tok.empty() && tok.back() == ' ') tok.pop_back();
            if (!tok.empty()) out.push_back(ParseHex(tok));
        }
        return out;
    }
}
