#pragma once
// ============================================================
//  CheatManager.h  –  Part of CTTrainer
// ============================================================
#include "CTParser.h"
#include "Memory.h"
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

enum class CheatType { DWORD_TYPE, FLOAT_TYPE, UNKNOWN };

struct Cheat
{
    // ── from CT file ──────────────────────────────────────
    std::string            description;
    std::string            moduleName;
    uintptr_t              baseOffset = 0;
    std::vector<uintptr_t> offsets;
    CheatType              type = CheatType::DWORD_TYPE;

    // ── runtime ───────────────────────────────────────────
    uintptr_t  resolvedAddr = 0;
    bool       resolved = false;

    // Live value read from process each frame
    DWORD      liveValueDW = 0;
    float      liveValueF = 0.0f;
    bool       liveReadOk = false;

    // Value to write / freeze
    DWORD      freezeValueDW = 0;
    float      freezeValueF = 0.0f;

    // Edit buffer for the "New Value" input field (per-cheat, persists between frames)
    char       editBuf[32] = "";
    bool       editActive = false;   // true while user is typing

    // Per-cheat freeze
    std::atomic<bool> freezeOn{ false };
    std::thread       freezeThread;

    Cheat() = default;
    Cheat(const Cheat&) = delete;
    Cheat& operator=(const Cheat&) = delete;
    Cheat(Cheat&&) = delete;
    Cheat& operator=(Cheat&&) = delete;
};

class CheatManager
{
public:
    CheatManager() = default;
    ~CheatManager();

    void SetProcess(HANDLE hProcess, DWORD pid);
    void SetIs32Bit(bool v) { m_is32Bit = v; }
    bool Is32Bit()    const { return m_is32Bit; }
    void LoadFromCT(const std::vector<CTEntry>& entries);

    void ResolveAll();
    void Resolve(int index);

    // Read current live value from process into cheat.liveValue*
    void ReadLive(int index);
    void ReadAllLive();

    // Write a value once (no freeze)
    void WriteOnce(int index);

    void StartFreeze(int index);
    void StopFreeze(int index);
    void StopAll();

    void MasterOn();
    void MasterOff();
    bool IsMasterOn() const { return m_masterOn.load(); }

    Cheat* GetCheat(int index);
    size_t Count()     const { return m_cheats.size(); }
    HANDLE HProcess()  const { return m_hProcess; }
    DWORD  PID()       const { return m_pid; }

private:
    HANDLE     m_hProcess = nullptr;
    DWORD      m_pid = 0;
    bool       m_is32Bit = true;   // true = 32-bit target, false = 64-bit target
    std::vector<std::unique_ptr<Cheat>> m_cheats;
    std::atomic<bool> m_masterOn{ false };

    void _FreezeLoop(int index);
    static CheatType TypeFromString(const std::string& s);
};