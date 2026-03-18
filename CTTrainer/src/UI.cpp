// ============================================================
//  UI.cpp  --  CTTrainer  (100% custom widgets, no ImGui defaults)
// ============================================================
#include "../include/UI.h"
#include "../include/Widgets.h"
#include "../include/CTParser.h"
#include "../include/Memory.h"
#include "../include/Logger.h"
#include "../include/Theme.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <Windows.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cmath>

// ── State ─────────────────────────────────────────────────────
static std::string s_statusMsg = "Load a .CT file and attach to begin.";
static bool        s_statusError = false;
static char        s_processName[64] = "";
static char        s_ctFilePath[256] = "";
static bool        s_showDebug    = false;
static bool        s_showSettings = false;
static bool        s_autoScroll   = true;
static bool        s_is32Bit      = true;
static int         s_alpha        = 255;   // window opacity 0-255
static bool        s_blurEnabled  = false;
static bool        s_acrylicEnabled = false;
static int         s_fontIndex    = 0;     // which loaded font is active

// One ValueCell::State per cheat row (index-mapped)
static std::vector<ValueCell::State> s_cellStates;

// From main.cpp
extern HWND g_hWnd;
// Font array and count loaded in main.cpp
extern ImFont* g_Fonts[4];
static const int k_UIFontCount = 4;
static const char* k_UIFontNames[] = { "Segoe UI", "Consolas", "Cascadia Code", "Arial" };
// Window effect helpers from main.cpp
extern void SetWindowAlpha(int);
extern void SetWindowBlur(bool);
extern void SetWindowAcrylic(bool);
extern void RebuildFontAtlas();

namespace UI {
    void SetStatus(const std::string& m, bool e)
    {
        s_statusMsg = m; s_statusError = e;
    }
}

// ── Helpers ───────────────────────────────────────────────────
static bool BrowseCT(char* out, int maxLen)
{
    OPENFILENAMEA ofn = {}; char f[260] = {};
    ofn.lStructSize = sizeof(ofn); ofn.lpstrFile = f; ofn.nMaxFile = sizeof(f);
    ofn.lpstrFilter = "Cheat Engine Table\0*.CT\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) { strncpy_s(out, maxLen, f, _TRUNCATE); return true; }
    return false;
}

static void FormatLive(const Cheat* c, char* buf, int len)
{
    if (!c->resolved || !c->liveReadOk)
    {
        strncpy_s(buf, len, "--", _TRUNCATE); return;
    }
    if (c->type == CheatType::FLOAT_TYPE)
        snprintf(buf, len, "%.3f", c->liveValueF);
    else
        snprintf(buf, len, "%lu", (unsigned long)c->liveValueDW);
}

static void CommitValue(CheatManager& mgr, int i, const std::string& str)
{
    Cheat* c = mgr.GetCheat(i); if (!c) return;
    try {
        if (c->type == CheatType::FLOAT_TYPE)
        {
            c->freezeValueF = std::stof(str); c->freezeValueDW = (DWORD)c->freezeValueF;
        }
        else
        {
            c->freezeValueDW = (DWORD)std::stoul(str, nullptr, 0); c->freezeValueF = (float)c->freezeValueDW;
        }
        if (!c->resolved) mgr.Resolve(i);
        if (c->resolved) { mgr.WriteOnce(i); UI::SetStatus("Written: " + c->description); }
        else UI::SetStatus(c->description + ": unresolved.", true);
    }
    catch (...) { UI::SetStatus("Invalid value.", true); }
}

// ── Section divider ───────────────────────────────────────────
static void Divider(float padTop = 6.f, float padBot = 6.f)
{
    const ThemePalette& p = ThemeManager::Get().Palette();
    ImGui::Dummy(ImVec2(0, padTop));
    ImVec2 a = ImGui::GetCursorScreenPos();
    float  w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(
        a, ImVec2(a.x + w, a.y),
        Anim::ColorU32(p.border), 1.f);
    ImGui::Dummy(ImVec2(0, padBot));
}

// ── Settings panel ────────────────────────────────────────────
static void RenderSettingsPanel(float panelY, float panelH, float w)
{
    if (!s_showSettings) return;
    const ThemePalette& p = ThemeManager::Get().Palette();
    ThemeManager& tm = ThemeManager::Get();
    const auto& allThemes = tm.All();

    ImGui::SetNextWindowPos(ImVec2(0, panelY));
    ImGui::SetNextWindowSize(ImVec2(w, panelH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, p.bg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.f, 16.f));
    ImGui::Begin("##settings", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor(); ImGui::PopStyleVar(2);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ─── Helper: section header ───────────────────────────────
    auto SectionHdr = [&](const char* title) {
        ImVec2 cp = ImGui::GetCursorScreenPos();
        dl->AddText(cp, Anim::ColorU32(p.accent), title);
        ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight()));
        Divider(2, 8);
    };

    // ─── THEME ────────────────────────────────────────────────
    SectionHdr("THEME");
    {
        float btnW = (w - 40.f - (float)(allThemes.size() - 1) * 8.f) / (float)allThemes.size();
        float btnH = 32.f;
        for (int i = 0; i < (int)allThemes.size(); ++i)
        {
            if (i > 0) ImGui::SameLine(0, 8);
            bool sel = (tm.Current() == i);
            ImVec4 bc  = sel ? p.accent : ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.35f);
            ImVec4 hf  = ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.18f);
            char lid[16]; snprintf(lid, sizeof(lid), "##ts%d", i);
            if (GhostButton::Draw(lid, allThemes[i].name, ImVec2(btnW, btnH), bc, hf, 0.f))
                tm.Apply(i);
            // Active indicator underline
            if (sel) {
                ImVec2 sp = ImGui::GetItemRectMin();
                ImVec2 ep = ImGui::GetItemRectMax();
                dl->AddLine(ImVec2(sp.x, ep.y - 2.f), ImVec2(ep.x, ep.y - 2.f),
                    Anim::ColorU32(p.accent), 2.f);
            }
        }
    }
    ImGui::Dummy(ImVec2(0, 12));

    // ─── FONT ─────────────────────────────────────────────────
    SectionHdr("FONT");
    {
        float btnW = (w - 40.f - 3.f * 8.f) / 4.f;
        for (int i = 0; i < k_UIFontCount; ++i)
        {
            if (i > 0) ImGui::SameLine(0, 8);
            bool avail = (g_Fonts[i] != nullptr);
            bool sel   = (s_fontIndex == i && avail);
            ImVec4 bc  = sel   ? p.accent :
                         avail ? ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.30f)
                               : ImVec4(p.textDisabled.x, p.textDisabled.y, p.textDisabled.z, 0.20f);
            ImVec4 hf  = avail ? ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.15f)
                               : ImVec4(0,0,0,0);
            char lid[16]; snprintf(lid, sizeof(lid), "##fn%d", i);
            if (GhostButton::Draw(lid, k_UIFontNames[i], ImVec2(btnW, 32.f), bc, hf, 0.f)
                && avail && i != s_fontIndex)
            {
                s_fontIndex = i;
                ImGui::GetIO().FontDefault = g_Fonts[i];
                RebuildFontAtlas();
            }
            if (sel) {
                ImVec2 sp = ImGui::GetItemRectMin();
                ImVec2 ep = ImGui::GetItemRectMax();
                dl->AddLine(ImVec2(sp.x, ep.y - 2.f), ImVec2(ep.x, ep.y - 2.f),
                    Anim::ColorU32(p.accent), 2.f);
            }
        }
    }
    ImGui::Dummy(ImVec2(0, 12));

    // ─── OPACITY ──────────────────────────────────────────────
    SectionHdr("OPACITY");
    {
        // Slider 77 (30%) to 255 (100%)
        int alpha = s_alpha;
        ImGui::PushStyleColor(ImGuiCol_SliderGrab,       p.accent);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, p.accentHover);
        ImGui::PushStyleColor(ImGuiCol_FrameBg,          p.inputBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   p.accentDim);
        ImGui::SetNextItemWidth(w - 40.f - 60.f);
        if (ImGui::SliderInt("##opacity", &alpha, 77, 255, ""))
        {
            s_alpha = alpha;
            SetWindowAlpha(s_alpha);
        }
        ImGui::PopStyleColor(4);
        ImGui::SameLine(0, 12);
        char pct[16]; snprintf(pct, sizeof(pct), "%d%%", (int)((alpha / 255.f) * 100.f));
        ImVec2 pp = ImGui::GetCursorScreenPos();
        dl->AddText(ImVec2(pp.x, pp.y + 3.f), Anim::ColorU32(p.textSecondary), pct);
        ImGui::Dummy(ImVec2(0, 22));
    }
    ImGui::Dummy(ImVec2(0, 4));

    // ─── EFFECTS ──────────────────────────────────────────────
    SectionHdr("WINDOW EFFECTS");
    {
        float bw = 130.f, bh = 32.f;
        // Blur toggle
        ImVec4 blurBc = s_blurEnabled
            ? p.accent : ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.32f);
        if (GhostButton::Draw("##blurtgl",
            s_blurEnabled ? "Blur  ON" : "Blur  OFF",
            ImVec2(bw, bh), blurBc,
            ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.15f), 0.f))
        {
            s_blurEnabled = !s_blurEnabled;
            if (s_blurEnabled) { s_acrylicEnabled = false; SetWindowAcrylic(false); }
            SetWindowBlur(s_blurEnabled);
        }
        ImGui::SameLine(0, 8);
        // Acrylic toggle
        ImVec4 acrBc = s_acrylicEnabled
            ? p.accent : ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.32f);
        if (GhostButton::Draw("##acrytgl",
            s_acrylicEnabled ? "Acrylic  ON" : "Acrylic  OFF",
            ImVec2(bw + 20.f, bh), acrBc,
            ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.15f), 0.f))
        {
            s_acrylicEnabled = !s_acrylicEnabled;
            if (s_acrylicEnabled) { s_blurEnabled = false; SetWindowBlur(false); }
            SetWindowAcrylic(s_acrylicEnabled);
        }

        if (s_blurEnabled || s_acrylicEnabled) {
            ImGui::Dummy(ImVec2(0, 6));
            ImVec2 cp = ImGui::GetCursorScreenPos();
            dl->AddText(cp, Anim::ColorU32(p.warn),
                "Tip: lower Opacity to see the effect behind the window.");
            ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight()));
        }
    }

    ImGui::End();
}

// ── Debug log panel ───────────────────────────────────────────
static void RenderDebugPanel()
{
    if (!s_showDebug) return;
    ImGuiIO& io = ImGui::GetIO();
    const ThemePalette& p = ThemeManager::Get().Palette();
    float panelH = 220.f, panelY = io.DisplaySize.y - panelH - 26.f;

    ImGui::SetNextWindowPos(ImVec2(0, panelY));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, panelH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, p.bgDeep);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::Begin("##dbg", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor(); ImGui::PopStyleVar();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 hp = ImGui::GetCursorScreenPos();
    dl->AddText(ImVec2(hp.x + 8, hp.y + 2), Anim::ColorU32(p.accent), "DEBUG LOG");
    ImGui::Dummy(ImVec2(0, 18));
    ImGui::SameLine(100);

    if (GhostButton::Draw("##clr", "Clear", ImVec2(54, 20), p.accent,
        ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.15f), 3.f))
        Logger::Get().Clear();
    ImGui::SameLine();
    if (GhostButton::Draw("##cpa", "Copy All", ImVec2(74, 20), p.accent,
        ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.15f), 3.f))
    {
        auto copy = Logger::Get().GetEntries();
        std::string all2; all2.reserve(copy.size() * 80);
        for (auto& e : copy) {
            switch (e.level) {
            case LogLevel::ERR:   all2 += "[ERR]  "; break;
            case LogLevel::WARN:  all2 += "[WARN] "; break;
            case LogLevel::DEBUG: all2 += "[DBG]  "; break;
            default:              all2 += "[INFO] "; break;
            }
            all2 += e.message + "\n";
        }
        ImGui::SetClipboardText(all2.c_str());
    }

    Divider(4, 4);

    ImGui::BeginChild("##ls", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    auto entries = Logger::Get().GetEntries();
    for (int i = 0; i < (int)entries.size(); ++i)
    {
        const auto& e = entries[i];
        ImVec4 col; const char* pfx = "";
        switch (e.level) {
        case LogLevel::ERR:   col = p.bad;          pfx = "[ERR]  "; break;
        case LogLevel::WARN:  col = p.warn;         pfx = "[WARN] "; break;
        case LogLevel::DEBUG: col = p.textDisabled; pfx = "[DBG]  "; break;
        default:              col = p.textSecondary; pfx = "[INFO] "; break;
        }
        std::string line = std::string(pfx) + e.message;
        // read-only selectable text via invisible InputText
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, p.accentDim);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        ImGui::SetNextItemWidth(-1.f);
        ImGui::PushID(i);
        char buf[1024]; strncpy_s(buf, sizeof(buf), line.c_str(), _TRUNCATE);
        ImGui::InputText("##l", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopID(); ImGui::PopStyleColor(4);
    }
    if (s_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f)
        ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();
    ImGui::End();
}

// ── Custom table header row ────────────────────────────────────
static void DrawTableHeader(float colWidths[4], const char* labels[4],
    float rowH)
{
    const ThemePalette& p = ThemeManager::Get().Palette();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float  w = ImGui::GetContentRegionAvail().x;

    // Header bg
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + rowH),
        Anim::ColorU32(p.tableHeader));
    // Bottom border of header
    dl->AddLine(ImVec2(pos.x, pos.y + rowH),
        ImVec2(pos.x + w, pos.y + rowH),
        Anim::ColorU32(p.border), 1.f);

    float x = pos.x + 8.f;
    for (int i = 0; i < 4; ++i)
    {
        ImVec2 tp(x + 2.f, pos.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f);
        dl->AddText(tp, Anim::ColorU32(p.textSecondary), labels[i]);
        // column divider
        if (i < 3) dl->AddLine(ImVec2(x + colWidths[i] - 1, pos.y),
            ImVec2(x + colWidths[i] - 1, pos.y + rowH),
            Anim::ColorU32(p.border), 1.f);
        x += colWidths[i];
    }
    ImGui::Dummy(ImVec2(w, rowH));
}

// ── Custom table row background ───────────────────────────────
static void DrawRowBg(int rowIndex, float rowH)
{
    const ThemePalette& p = ThemeManager::Get().Palette();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float  w = ImGui::GetContentRegionAvail().x;
    ImVec4 bgc = (rowIndex % 2 == 0) ? p.tableRowEven : p.tableRowOdd;
    ImGui::GetWindowDrawList()->AddRectFilled(
        pos, ImVec2(pos.x + w, pos.y + rowH), Anim::ColorU32(bgc));
}

// ── Cheat table ───────────────────────────────────────────────
static void RenderCheatTable(CheatManager& mgr, float availH)
{
    if (mgr.HProcess()) mgr.ReadAllLive();
    const ThemePalette& p = ThemeManager::Get().Palette();

    // Keep cell states in sync with cheat count
    while ((int)s_cellStates.size() < (int)mgr.Count())
        s_cellStates.push_back(ValueCell::State{});
    if ((int)s_cellStates.size() > (int)mgr.Count())
        s_cellStates.resize(mgr.Count());

    float totalW = ImGui::GetContentRegionAvail().x;
    float freezeW = 76.f;
    float stateW = 56.f;
    float nameW = totalW * 0.36f;
    float valueW = totalW - nameW - freezeW - stateW;
    float colW[4] = { nameW, valueW, freezeW, stateW };
    const char* hdrs[4] = { "Name", "Value", "Freeze", "State" };
    const float ROW_H = 28.f;
    const float HDR_H = 26.f;

    // Outer scrollable child
    ImGui::BeginChild("##tbl", ImVec2(0, availH), false,
        ImGuiWindowFlags_NoScrollbar);

    DrawTableHeader(colW, hdrs, HDR_H);

    // Row area
    ImGui::BeginChild("##rows", ImVec2(0, 0), false);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < (int)mgr.Count(); ++i)
    {
        Cheat* c = mgr.GetCheat(i); if (!c) continue;
        bool   frozen = c->freezeOn.load();

        ImGui::PushID(i);
        DrawRowBg(i, ROW_H);
        ImVec2 rowStart = ImGui::GetCursorScreenPos();

        // ── Name cell ────────────────────────────────────────
        float x = rowStart.x + 8.f;
        ImVec4 nameCol = frozen ? p.frozen : p.textPrimary;
        dl->AddText(ImVec2(x, rowStart.y + (ROW_H - ImGui::GetTextLineHeight()) * 0.5f),
            Anim::ColorU32(nameCol), c->description.c_str());

        // ── Value cell (custom ValueCell widget) ──────────────
        ImGui::SetCursorScreenPos(ImVec2(rowStart.x + nameW, rowStart.y + 3.f));
        char liveBuf[32]; FormatLive(c, liveBuf, sizeof(liveBuf));
        std::string committed;
        if (ValueCell::Draw(("##vc" + std::to_string(i)).c_str(),
            s_cellStates[i], liveBuf, frozen,
            valueW - 8.f, committed))
            CommitValue(mgr, i, committed);

        // ── Freeze / Thaw (PillButton) ────────────────────────
        ImGui::SetCursorScreenPos(
            ImVec2(rowStart.x + nameW + valueW + 4.f,
                rowStart.y + (ROW_H - 22.f) * 0.5f));

        if (PillButton::Draw("##fr", "Thaw", "Freeze", frozen,
            ImVec2(freezeW - 8.f, 22.f),
            p.btnThaw, p.btnThawHov,
            p.btnFreeze, p.btnFreezeHov))
        {
            if (frozen) { mgr.StopFreeze(i); UI::SetStatus(c->description + " unfrozen."); }
            else {
                if (!mgr.HProcess()) UI::SetStatus("Attach first.", true);
                else {
                    if (c->liveReadOk)
                    {
                        c->freezeValueDW = c->liveValueDW; c->freezeValueF = c->liveValueF;
                    }
                    if (!c->resolved) mgr.Resolve(i);
                    if (!c->resolved) UI::SetStatus(c->description + ": unresolved.", true);
                    else { mgr.StartFreeze(i); UI::SetStatus(c->description + " frozen."); }
                }
            }
        }

        // ── State indicator ───────────────────────────────────
        ImVec2 statePos(rowStart.x + nameW + valueW + freezeW + 4.f,
            rowStart.y + (ROW_H - ImGui::GetTextLineHeight()) * 0.5f);
        if (!c->resolved)
        {
            dl->AddText(statePos, Anim::ColorU32(p.textDisabled), "  ?");
        }
        else if (frozen)
        {
            // Pulsing dot + text
            float pulse = (sinf((float)ImGui::GetTime() * 2.5f) + 1.f) * 0.5f;
            ImU32 dc = Anim::ColorU32(ImVec4(p.frozen.x, p.frozen.y, p.frozen.z,
                0.5f + 0.5f * pulse));
            dl->AddCircleFilled(ImVec2(statePos.x + 4.f,
                statePos.y + ImGui::GetTextLineHeight() * 0.5f),
                3.f, dc);
            dl->AddText(ImVec2(statePos.x + 12.f, statePos.y),
                Anim::ColorU32(p.frozen), "ON");
        }
        else
        {
            dl->AddText(statePos, Anim::ColorU32(p.textDisabled), "idle");
        }

        // Row divider line
        dl->AddLine(ImVec2(rowStart.x, rowStart.y + ROW_H),
            ImVec2(rowStart.x + totalW, rowStart.y + ROW_H),
            Anim::ColorU32(p.border), 1.f);

        // Advance cursor past the row
        ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + ROW_H));
        ImGui::Dummy(ImVec2(totalW, 0.f));

        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

// ── Custom title bar (36 px, full-width, draws in screen space) ─
static void RenderTitleBar(CheatManager& mgr)
{
    const ThemePalette& p   = ThemeManager::Get().Palette();
    ImDrawList*         dl  = ImGui::GetWindowDrawList();
    ImVec2              wp  = ImGui::GetWindowPos();   // always (0,0)
    float               fw  = ImGui::GetWindowSize().x;
    const float         H   = 36.f;

    // ── Background + bottom accent border ────────────────────
    dl->AddRectFilled(wp, ImVec2(wp.x + fw, wp.y + H), Anim::ColorU32(p.bgDeep));
    dl->AddLine(
        ImVec2(wp.x,      wp.y + H - 1.f),
        ImVec2(wp.x + fw, wp.y + H - 1.f),
        Anim::ColorU32(ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.6f)), 1.5f);

    // ── Icon bars + label ─────────────────────────────────────
    float ix = wp.x + 14.f, iy = wp.y + (H - 10.f) * 0.5f;
    dl->AddRectFilled(ImVec2(ix,        iy),        ImVec2(ix + 3.f,  iy + 10.f), Anim::ColorU32(p.accent));
    dl->AddRectFilled(ImVec2(ix + 5.f,  iy + 3.f),  ImVec2(ix + 8.f,  iy + 10.f), Anim::ColorU32(p.accent));
    dl->AddRectFilled(ImVec2(ix + 10.f, iy),        ImVec2(ix + 13.f, iy + 10.f), Anim::ColorU32(p.accent));
    float titleEndX = ix + 13.f + 10.f;
    dl->AddText(ImVec2(titleEndX, wp.y + (H - ImGui::GetTextLineHeight()) * 0.5f),
        Anim::ColorU32(p.accent), "CT TRAINER");
    titleEndX += ImGui::CalcTextSize("CT TRAINER").x + 10.f;

    // ── Close [x] button ─────────────────────────────────────
    const float BW = 36.f;
    float cx = wp.x + fw - BW;
    ImGui::SetCursorScreenPos(ImVec2(cx, wp.y));
    ImGui::InvisibleButton("##wclose", ImVec2(BW, H));
    bool cHov = ImGui::IsItemHovered(), cCli = ImGui::IsItemClicked();
    dl->AddRectFilled(ImVec2(cx, wp.y), ImVec2(cx + BW, wp.y + H),
        cHov ? IM_COL32(210, 30, 30, 230) : IM_COL32(0, 0, 0, 0));
    { ImVec2 s = ImGui::CalcTextSize("x");
      dl->AddText(ImVec2(cx + (BW - s.x)*0.5f, wp.y + (H - s.y)*0.5f),
        cHov ? IM_COL32(255,255,255,255) : Anim::ColorU32(p.textSecondary), "x"); }
    if (cCli) PostQuitMessage(0);

    // ── Minimize [_] button ───────────────────────────────────
    float mx = cx - BW;
    ImGui::SetCursorScreenPos(ImVec2(mx, wp.y));
    ImGui::InvisibleButton("##wmini", ImVec2(BW, H));
    bool mHov = ImGui::IsItemHovered(), mCli = ImGui::IsItemClicked();
    dl->AddRectFilled(ImVec2(mx, wp.y), ImVec2(mx + BW, wp.y + H),
        mHov ? Anim::ColorU32(ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.22f)) : IM_COL32(0,0,0,0));
    { ImVec2 s = ImGui::CalcTextSize("_");
      dl->AddText(ImVec2(mx + (BW - s.x)*0.5f, wp.y + (H - s.y)*0.5f - 2.f),
        mHov ? Anim::ColorU32(p.accent) : Anim::ColorU32(p.textSecondary), "_"); }
    if (mCli) ShowWindow(g_hWnd, SW_MINIMIZE);

    // ── PID indicator (left of system buttons) ────────────────
    float pulse   = (sinf((float)ImGui::GetTime() * 2.f) + 1.f) * 0.5f;
    ImVec4 pidCol = mgr.HProcess() ? p.good : p.bad;
    char pidBuf[32];
    if (mgr.HProcess()) snprintf(pidBuf, sizeof(pidBuf), "PID %lu", (unsigned long)mgr.PID());
    else strncpy_s(pidBuf, sizeof(pidBuf), "DETACHED", _TRUNCATE);
    ImVec2 pidSz  = ImGui::CalcTextSize(pidBuf);
    float  pidTx  = mx - 12.f - pidSz.x;
    float  pidDot = pidTx - 12.f;
    dl->AddCircleFilled(ImVec2(pidDot, wp.y + H * 0.5f),
        4.f, Anim::ColorU32(ImVec4(pidCol.x, pidCol.y, pidCol.z, 0.5f + 0.5f * pulse)));
    dl->AddText(ImVec2(pidTx, wp.y + (H - pidSz.y) * 0.5f),
        Anim::ColorU32(pidCol), pidBuf);

    // ── Settings + Log ghost buttons ─────────────────────────────
    const float GBH = 22.f, GBY = wp.y + (H - GBH) * 0.5f;
    float logW = 72.f, thW = 90.f;
    float logX = pidDot - 16.f - logW;
    float thX  = logX - 8.f - thW;

    ImGui::SetCursorScreenPos(ImVec2(thX, GBY));
    {
        ImVec4 bc = s_showSettings
            ? p.accent
            : ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.35f);
        if (GhostButton::Draw("##setbtn", s_showSettings ? "Settings [x]" : "Settings",
            ImVec2(thW, GBH), bc,
            ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.12f), 0.f))
            s_showSettings = !s_showSettings;
    }

    ImGui::SetCursorScreenPos(ImVec2(logX, GBY));
    if (GhostButton::Draw("##dbgbtn", s_showDebug ? "Log [x]" : "Log [ ]",
        ImVec2(logW, GBH), p.textSecondary,
        ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.10f), 0.f))
        s_showDebug = !s_showDebug;

    // ── Drag zone (middle of title bar) ──────────────────────
    // Uses per-frame SetWindowPos to avoid the SendMessage modal-loop bug.
    // (SendMessage(WM_NCLBUTTONDOWN) blocks until mouse release and Windows
    //  consumes WM_LBUTTONUP, leaving ImGui with a permanently-stuck MouseDown
    //  which breaks every other drag attempt.)
    static bool  s_dragActive = false;
    static POINT s_dragOriginCursor = {};
    static POINT s_dragOriginWin    = {};

    float dragW = thX - wp.x - 8.f;
    if (dragW > 0.f) {
        ImGui::SetCursorScreenPos(ImVec2(wp.x, wp.y));
        ImGui::InvisibleButton("##wdrag", ImVec2(dragW, H));

        if (ImGui::IsItemActivated()) {
            // Capture starting positions on first press
            GetCursorPos(&s_dragOriginCursor);
            RECT wr = {};
            GetWindowRect(g_hWnd, &wr);
            s_dragOriginWin = { wr.left, wr.top };
            s_dragActive = true;
        }
    }

    // Move window every frame while held (even if cursor leaves the zone)
    if (s_dragActive) {
        if (ImGui::IsMouseDown(0)) {
            POINT cur = {};
            GetCursorPos(&cur);
            SetWindowPos(g_hWnd, nullptr,
                s_dragOriginWin.x + (cur.x - s_dragOriginCursor.x),
                s_dragOriginWin.y + (cur.y - s_dragOriginCursor.y),
                0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        } else {
            s_dragActive = false;  // mouse released – end drag
        }
    }

    // Advance ImGui layout cursor past the title bar
    ImGui::SetCursorScreenPos(ImVec2(wp.x + 12.f, wp.y + H));
    ImGui::Dummy(ImVec2(fw - 24.f, 0.f));
    Divider(6, 6);
}


// ── Arch toggle pill button  (32-bit  ↔  64-bit) ─────────────
static void DrawArchToggle(float x, float y)
{
    const ThemePalette& p = ThemeManager::Get().Palette();
    ImDrawList* dl        = ImGui::GetWindowDrawList();

    const float BW = 96.f, BH = 28.f, R = BH * 0.5f;
    ImVec2      tl(x, y), br(x + BW, y + BH);

    // Draw pill background
    ImVec4 bgCol = s_is32Bit
        ? ImVec4(p.btnMasterOff.x, p.btnMasterOff.y, p.btnMasterOff.z, 0.85f)
        : ImVec4(p.btnAction.x,    p.btnAction.y,    p.btnAction.z,    0.85f);
    dl->AddRectFilled(tl, br, Anim::ColorU32(bgCol), R);

    // Glowing border
    ImVec4 borderCol = s_is32Bit ? p.btnMasterOffHov : p.accent;
    dl->AddRect(tl, br, Anim::ColorU32(borderCol), R, 0, 1.5f);

    // Sliding thumb knob
    float thumbPad = 3.f, thumbD = BH - thumbPad * 2.f;
    float thumbX   = s_is32Bit ? tl.x + thumbPad
                               : br.x - thumbPad - thumbD;
    dl->AddCircleFilled(
        ImVec2(thumbX + thumbD * 0.5f, y + BH * 0.5f),
        thumbD * 0.5f,
        Anim::ColorU32(ImVec4(1.f, 1.f, 1.f, 0.92f)));

    // Label text
    const char* lbl = s_is32Bit ? "32-bit" : "64-bit";
    ImVec2 ts = ImGui::CalcTextSize(lbl);
    float  tx = s_is32Bit ? tl.x + thumbD + thumbPad + 6.f
                          : tl.x + 8.f;
    float  ty = y + (BH - ts.y) * 0.5f;
    dl->AddText(ImVec2(tx, ty), Anim::ColorU32(p.textPrimary), lbl);

    // Invisible clickable overlay
    ImGui::SetCursorScreenPos(tl);
    ImGui::InvisibleButton("##arch", ImVec2(BW, BH));
    if (ImGui::IsItemClicked())
        s_is32Bit = !s_is32Bit;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Toggle process architecture\n32-bit = older games  |  64-bit = modern games");
}

// ── Toolbar: two-row layout ────────────────────────────────────
static void RenderToolbar(CheatManager& mgr)
{
    const ThemePalette& p = ThemeManager::Get().Palette();

    // ── ROW 1 : Process  |  Attach  |  Arch toggle ───────────
    TextInput::Draw("##proc", s_processName, sizeof(s_processName),
        160.f, "Process name...", p.textSecondary);
    ImGui::SameLine(0, 8);

    if (GlowButton::Draw("##att", "Attach", ImVec2(70, 28),
        p.btnAction, p.btnActionHov, p.accent))
    {
        mgr.StopAll();
        mgr.SetIs32Bit(s_is32Bit);
        LOG_INFO("--- Attach: '" + std::string(s_processName) + "' (" +
            (s_is32Bit ? "32-bit" : "64-bit") + ") ---");
        std::wstring wp(s_processName, s_processName + strlen(s_processName));
        DWORD pid = Memory::GetProcessID(wp);
        if (!pid) UI::SetStatus("Not found: " + std::string(s_processName), true);
        else {
            HANDLE h = Memory::OpenTarget(pid);
            if (!h) UI::SetStatus("OpenProcess failed - run as Admin!", true);
            else {
                mgr.SetProcess(h, pid); mgr.ResolveAll();
                UI::SetStatus("Attached PID " + std::to_string(pid));
            }
        }
    }

    // Arch toggle pill
    ImGui::SameLine(0, 10);
    {
        ImVec2 cur = ImGui::GetCursorScreenPos();
        DrawArchToggle(cur.x, cur.y);
        // Advance cursor so ImGui layout knows we used space
        ImGui::SetCursorScreenPos(ImVec2(cur.x + 96.f + 10.f, cur.y));
        ImGui::Dummy(ImVec2(0, 28));
    }

    // ── Thin separator between rows ───────────────────────────
    ImGui::Dummy(ImVec2(0, 4));

    // ── ROW 2 : CT path  |  Browse  |  Load CT ───────────────
    {
        // Small muted "CT File:" label
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      lp  = ImGui::GetCursorScreenPos();
        const char* lbl = "CT File:";
        ImVec2      lsz = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(lp.x, lp.y + (28.f - lsz.y) * 0.5f),
            Anim::ColorU32(p.textSecondary), lbl);
        ImGui::SetCursorScreenPos(ImVec2(lp.x + lsz.x + 8.f, lp.y));
    }

    float browseW = 72.f;
    float loadCtW = 76.f;
    // avail is measured from cursor (already past the "CT File:" label)
    float avail   = ImGui::GetContentRegionAvail().x;
    float ctPathW = avail - browseW - loadCtW - 6.f - 6.f;

    TextInput::Draw("##ctp", s_ctFilePath, sizeof(s_ctFilePath),
        ctPathW, "Path to .CT file...", p.textSecondary);
    ImGui::SameLine(0, 6);

    if (GhostButton::Draw("##br", "Browse", ImVec2(browseW, 28),
        p.accent, ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.12f), 4.f))
        BrowseCT(s_ctFilePath, sizeof(s_ctFilePath));
    ImGui::SameLine(0, 6);

    if (GlowButton::Draw("##lct", "Load CT", ImVec2(loadCtW, 28),
        p.btnAction, p.btnActionHov, p.accent))
    {
        if (!s_ctFilePath[0]) UI::SetStatus("Select a .CT file.", true);
        else {
            LOG_INFO("--- Load CT: '" + std::string(s_ctFilePath) + "' ---");
            auto entries = CTParser::Load(s_ctFilePath);
            if (entries.empty()) UI::SetStatus("No entries found.", true);
            else {
                mgr.LoadFromCT(entries);
                s_cellStates.clear();
                if (mgr.HProcess()) mgr.ResolveAll();
                UI::SetStatus("Loaded " + std::to_string(entries.size()) + " entries.");
            }
        }
    }
}

// ── Master freeze button ──────────────────────────────────────
static void RenderMasterToggle(CheatManager& mgr)
{
    const ThemePalette& p = ThemeManager::Get().Palette();
    bool masterOn = mgr.IsMasterOn();
    float bw = 200.f;
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - bw) * 0.5f);

    if (PillButton::Draw("##mall",
        "DISABLE ALL", "FREEZE ALL",
        masterOn, ImVec2(bw, 32),
        p.btnMasterOn, p.btnMasterOnHov,
        p.btnMasterOff, p.btnMasterOffHov))
    {
        if (!mgr.HProcess()) UI::SetStatus("Attach first.", true);
        else if (!mgr.Count()) UI::SetStatus("Load a CT file first.", true);
        else if (masterOn) { mgr.MasterOff(); UI::SetStatus("All cheats off."); }
        else { mgr.MasterOn();  UI::SetStatus("All frozen!"); }
    }
}

// ── Main render ───────────────────────────────────────────────
void UI::Render(CheatManager& mgr)
{
    ImGuiIO& io = ImGui::GetIO();
    const ThemePalette& p = ThemeManager::Get().Palette();

    float debugH = s_showDebug ? 228.f : 0.f;
    float mainH  = io.DisplaySize.y - debugH - 26.f;
    float w      = io.DisplaySize.x;
    const float TITLE_H = 56.f; // title bar (36) + divider (~20)

    // -- Main window --
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(w, mainH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, p.bg);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::Begin("##main", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor(2); ImGui::PopStyleVar(2);

    RenderTitleBar(mgr);

    if (!s_showSettings)
    {
        RenderToolbar(mgr);
        Divider(8, 8);
        RenderMasterToggle(mgr);

        if (mgr.Count() > 0)
        {
            ImGui::Spacing();
            float tableH = mainH - ImGui::GetCursorPosY() - 8.f;
            RenderCheatTable(mgr, tableH);
        }
        else
        {
            ImGui::Dummy(ImVec2(0, 40));
            const char* hint = "Load a .CT file to see entries.";
            float hw = ImGui::CalcTextSize(hint).x;
            ImGui::SetCursorPosX((w - hw) * 0.5f);
            ImGui::GetWindowDrawList()->AddText(
                ImGui::GetCursorScreenPos(),
                Anim::ColorU32(p.textDisabled), hint);
            ImGui::Dummy(ImVec2(0, 20));
        }
    }

    ImGui::End();

    // Settings panel sits below the title bar as a separate overlay
    RenderSettingsPanel(TITLE_H, mainH - TITLE_H, w);
    RenderDebugPanel();
    StatusBar::Draw(io.DisplaySize.y - 26.f, w, 26.f,
        s_statusMsg.c_str(), s_statusError);
}
