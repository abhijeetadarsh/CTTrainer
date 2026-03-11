// ============================================================
//  CheatManager.cpp  –  Part of CTTrainer
// ============================================================
#include "../include/CheatManager.h"
#include "../include/Logger.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>

static std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

CheatType CheatManager::TypeFromString(const std::string& s)
{
    if (ToLower(s).find("float") != std::string::npos) return CheatType::FLOAT_TYPE;
    return CheatType::DWORD_TYPE;
}

// ── Setup ────────────────────────────────────────────────────

void CheatManager::SetProcess(HANDLE hProcess, DWORD pid)
{
    StopAll();
    m_hProcess = hProcess;
    m_pid = pid;
}

void CheatManager::LoadFromCT(const std::vector<CTEntry>& entries)
{
    StopAll();
    m_cheats.clear();

    for (const auto& e : entries)
    {
        auto c = std::make_unique<Cheat>();
        c->description = e.description;
        c->moduleName = e.moduleName;
        c->baseOffset = e.baseOffset;
        c->offsets = e.offsets;
        c->type = TypeFromString(e.varType);

        std::string low = ToLower(c->description);
        if (low.find("health") != std::string::npos ||
            low.find("stamina") != std::string::npos)
        {
            c->freezeValueDW = 100;    c->freezeValueF = 100.0f;
        }
        else if (low.find("ammo") != std::string::npos)
        {
            c->freezeValueDW = 999;    c->freezeValueF = 999.0f;
        }
        else if (low.find("money") != std::string::npos ||
            low.find("cash") != std::string::npos)
        {
            c->freezeValueDW = 999999; c->freezeValueF = 999999.0f;
        }
        else if (low.find("speed") != std::string::npos)
        {
            c->freezeValueF = 2.0f;    c->freezeValueDW = 2;
        }

        // Pre-fill edit buffer with default freeze value
        if (c->type == CheatType::FLOAT_TYPE)
            snprintf(c->editBuf, sizeof(c->editBuf), "%.2f", c->freezeValueF);
        else
            snprintf(c->editBuf, sizeof(c->editBuf), "%lu",
                (unsigned long)c->freezeValueDW);

        m_cheats.push_back(std::move(c));
    }
}

Cheat* CheatManager::GetCheat(int index)
{
    if (index < 0 || index >= (int)m_cheats.size()) return nullptr;
    return m_cheats[index].get();
}

// ── Resolve ──────────────────────────────────────────────────

void CheatManager::Resolve(int index)
{
    if (!m_hProcess || index < 0 || index >= (int)m_cheats.size()) return;
    Cheat& c = *m_cheats[index];
    c.resolved = false;
    c.resolvedAddr = 0;
    c.liveReadOk = false;

    char hexBuf[32];
    LOG_DEBUG("Resolve[" + std::to_string(index) + "] desc='" + c.description +
        "' module='" + c.moduleName + "'");

    uintptr_t base = c.baseOffset;
    if (!c.moduleName.empty())
    {
        std::wstring wMod(c.moduleName.begin(), c.moduleName.end());
        uintptr_t modBase = Memory::GetModuleBase(m_pid, wMod);
        if (!modBase)
        {
            LOG_ERR("Resolve[" + std::to_string(index) + "] module base = 0, aborting");
            return;
        }
        snprintf(hexBuf, sizeof(hexBuf), "0x%llX", (unsigned long long)modBase);
        LOG_DEBUG("Resolve[" + std::to_string(index) + "] modBase=" + hexBuf);
        base += modBase;
    }

    snprintf(hexBuf, sizeof(hexBuf), "0x%llX", (unsigned long long)base);
    LOG_DEBUG("Resolve[" + std::to_string(index) + "] effective base = " + hexBuf +
        "  offsets count = " + std::to_string(c.offsets.size()));

    c.resolvedAddr = c.offsets.empty()
        ? base
        : Memory::ResolveChain(m_hProcess, base, c.offsets, m_is32Bit);

    c.resolved = (c.resolvedAddr != 0);

    if (c.resolved)
    {
        snprintf(hexBuf, sizeof(hexBuf), "0x%llX", (unsigned long long)c.resolvedAddr);
        LOG_INFO("Resolve[" + std::to_string(index) + "] SUCCESS -> " + hexBuf);
    }
    else
        LOG_ERR("Resolve[" + std::to_string(index) + "] FAILED for '" + c.description + "'");
}

void CheatManager::ResolveAll()
{
    for (int i = 0; i < (int)m_cheats.size(); ++i)
        Resolve(i);
}

// ── Live read ────────────────────────────────────────────────

void CheatManager::ReadLive(int index)
{
    if (!m_hProcess || index < 0 || index >= (int)m_cheats.size()) return;
    Cheat& c = *m_cheats[index];
    if (!c.resolved || !c.resolvedAddr) { c.liveReadOk = false; return; }

    if (c.type == CheatType::FLOAT_TYPE)
    {
        float val = 0.0f;
        c.liveReadOk = Memory::Read<float>(m_hProcess, c.resolvedAddr, val);
        if (c.liveReadOk) c.liveValueF = val;
    }
    else
    {
        DWORD val = 0;
        c.liveReadOk = Memory::Read<DWORD>(m_hProcess, c.resolvedAddr, val);
        if (c.liveReadOk) c.liveValueDW = val;
    }
}

void CheatManager::ReadAllLive()
{
    for (int i = 0; i < (int)m_cheats.size(); ++i)
        ReadLive(i);
}

// ── Write once ───────────────────────────────────────────────

void CheatManager::WriteOnce(int index)
{
    if (!m_hProcess || index < 0 || index >= (int)m_cheats.size()) return;
    Cheat& c = *m_cheats[index];
    if (!c.resolved) { Resolve(index); }
    if (!c.resolved || !c.resolvedAddr) return;

    if (c.type == CheatType::FLOAT_TYPE)
        Memory::Write<float>(m_hProcess, c.resolvedAddr, c.freezeValueF);
    else
        Memory::Write<DWORD>(m_hProcess, c.resolvedAddr, c.freezeValueDW);

    LOG_INFO("WriteOnce[" + std::to_string(index) + "] '" + c.description + "' written");
}

// ── Freeze loop ──────────────────────────────────────────────

void CheatManager::_FreezeLoop(int index)
{
    Cheat& c = *m_cheats[index];
    while (c.freezeOn.load())
    {
        if (m_hProcess && c.resolvedAddr)
        {
            // Re-resolve every tick to follow dynamic pointers
            Resolve(index);
            if (c.resolvedAddr)
            {
                if (c.type == CheatType::FLOAT_TYPE)
                    Memory::Write<float>(m_hProcess, c.resolvedAddr, c.freezeValueF);
                else
                    Memory::Write<DWORD>(m_hProcess, c.resolvedAddr, c.freezeValueDW);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void CheatManager::StartFreeze(int index)
{
    if (index < 0 || index >= (int)m_cheats.size()) return;
    Cheat& c = *m_cheats[index];
    StopFreeze(index);
    c.freezeOn = true;
    c.freezeThread = std::thread(&CheatManager::_FreezeLoop, this, index);
}

void CheatManager::StopFreeze(int index)
{
    if (index < 0 || index >= (int)m_cheats.size()) return;
    Cheat& c = *m_cheats[index];
    c.freezeOn = false;
    if (c.freezeThread.joinable())
        c.freezeThread.join();
}

void CheatManager::StopAll()
{
    m_masterOn = false;
    for (int i = 0; i < (int)m_cheats.size(); ++i)
        StopFreeze(i);
}

void CheatManager::MasterOn()
{
    m_masterOn = true;
    for (int i = 0; i < (int)m_cheats.size(); ++i)
        if (!m_cheats[i]->freezeOn.load())
            StartFreeze(i);
}

void CheatManager::MasterOff()
{
    m_masterOn = false;
    StopAll();
}

CheatManager::~CheatManager()
{
    StopAll();
}