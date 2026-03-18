// ============================================================
//  UI.cpp  --  CTTrainer  (sidebar + tabbed layout matching index.html)
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

// ── Tab enum ─────────────────────────────────────────────────
enum ActiveTab { TAB_TRAINER = 0, TAB_SETTINGS, TAB_LOG };

// ── State ─────────────────────────────────────────────────────
static std::string s_statusMsg = "Load a .CT file and attach to begin.";
static bool        s_statusError = false;
static char        s_processName[64] = "";
static char        s_ctFilePath[256] = "";
static bool        s_autoScroll   = true;
static bool        s_is32Bit      = true;
static int         s_alpha        = 255;
static bool        s_blurEnabled  = false;
static bool        s_acrylicEnabled = false;
static int         s_fontIndex    = 0;
static ActiveTab   s_activeTab    = TAB_TRAINER;

static std::vector<ValueCell::State> s_cellStates;

extern HWND g_hWnd;
extern ImFont* g_Fonts[4];
static const int k_UIFontCount = 4;
static const char* k_UIFontNames[] = { "Segoe UI", "Consolas", "Cascadia Code", "Arial" };
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

// ── Color helpers ─────────────────────────────────────────────
static ImVec4 Dim(ImVec4 c, float a) { return ImVec4(c.x, c.y, c.z, a); }
static ImVec4 Brighten(ImVec4 c, float f) {
    return ImVec4(fminf(c.x * f, 1.f), fminf(c.y * f, 1.f), fminf(c.z * f, 1.f), c.w);
}
static ImVec4 Mix(ImVec4 a, ImVec4 b, float t) { return Anim::LerpColor(a, b, t); }

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
    { strncpy_s(buf, len, "--", _TRUNCATE); return; }
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
        { c->freezeValueF = std::stof(str); c->freezeValueDW = (DWORD)c->freezeValueF; }
        else
        { c->freezeValueDW = (DWORD)std::stoul(str, nullptr, 0); c->freezeValueF = (float)c->freezeValueDW; }
        if (!c->resolved) mgr.Resolve(i);
        if (c->resolved) { mgr.WriteOnce(i); UI::SetStatus("Written: " + c->description); }
        else UI::SetStatus(c->description + ": unresolved.", true);
    }
    catch (...) { UI::SetStatus("Invalid value.", true); }
}

// ── Divider ───────────────────────────────────────────────────
static void Divider(float padTop = 6.f, float padBot = 6.f)
{
    const ThemePalette& p = ThemeManager::Get().Palette();
    ImGui::Dummy(ImVec2(0, padTop));
    ImVec2 a = ImGui::GetCursorScreenPos();
    float  w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(
        a, ImVec2(a.x + w, a.y), Anim::ColorU32(p.border), 1.f);
    ImGui::Dummy(ImVec2(0, padBot));
}

// ── Toggle Switch (34x18) ─────────────────────────────────────
static bool DrawToggleSwitch(const char* id, bool* value)
{
    const ThemePalette& p = ThemeManager::Get().Palette();
    ImGuiID wid = ImGui::GetID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float W = 34.f, H = 18.f;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(W, H));
    bool clicked = ImGui::IsItemClicked();
    if (clicked) *value = !(*value);

    float t = Anim::Towards(wid, *value ? 1.f : 0.f, 8.f);

    ImVec4 trackCol = Mix(Dim(p.border, 0.6f), Dim(p.accent, 0.25f), t);
    ImVec4 borderCol = Mix(p.border, Dim(p.accent, 0.6f), t);
    dl->AddRectFilled(pos, ImVec2(pos.x + W, pos.y + H), Anim::ColorU32(trackCol), 99.f);
    dl->AddRect(pos, ImVec2(pos.x + W, pos.y + H), Anim::ColorU32(borderCol), 99.f, 0, 1.f);

    float thumbD = 12.f;
    float thumbX = pos.x + 3.f + t * (W - thumbD - 6.f);
    ImVec4 thumbCol = Mix(p.textDisabled, p.accent, t);
    dl->AddCircleFilled(ImVec2(thumbX + thumbD * 0.5f, pos.y + H * 0.5f),
        thumbD * 0.5f, Anim::ColorU32(thumbCol));

    return clicked;
}

// ── Sidebar Icon Drawing ──────────────────────────────────────
static void DrawGridIcon(ImDrawList* dl, ImVec2 c, float s, ImU32 col)
{
    float g = s * 0.15f, sz = (s - g) * 0.5f;
    float x0 = c.x - s * 0.5f, y0 = c.y - s * 0.5f;
    dl->AddRect(ImVec2(x0, y0), ImVec2(x0 + sz, y0 + sz), col, 2.f, 0, 1.5f);
    dl->AddRect(ImVec2(x0 + sz + g, y0), ImVec2(x0 + s, y0 + sz), col, 2.f, 0, 1.5f);
    dl->AddRect(ImVec2(x0, y0 + sz + g), ImVec2(x0 + sz, y0 + s), col, 2.f, 0, 1.5f);
    dl->AddRect(ImVec2(x0 + sz + g, y0 + sz + g), ImVec2(x0 + s, y0 + s), col, 2.f, 0, 1.5f);
}

static void DrawGearIcon(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddCircle(c, r * 0.45f, col, 0, 1.5f);
    for (int i = 0; i < 8; ++i) {
        float a = (float)i * 3.14159f * 2.f / 8.f;
        dl->AddLine(
            ImVec2(c.x + cosf(a) * r * 0.55f, c.y + sinf(a) * r * 0.55f),
            ImVec2(c.x + cosf(a) * r * 0.85f, c.y + sinf(a) * r * 0.85f),
            col, 1.5f);
    }
}

static void DrawLogIcon(ImDrawList* dl, ImVec2 c, float s, ImU32 col)
{
    float hs = s * 0.5f;
    dl->AddRect(ImVec2(c.x - hs, c.y - hs), ImVec2(c.x + hs, c.y + hs), col, 3.f, 0, 1.5f);
    dl->AddLine(ImVec2(c.x - hs * 0.5f, c.y - hs * 0.25f), ImVec2(c.x + hs * 0.5f, c.y - hs * 0.25f), col, 1.5f);
    dl->AddLine(ImVec2(c.x - hs * 0.5f, c.y + hs * 0.15f), ImVec2(c.x + hs * 0.3f, c.y + hs * 0.15f), col, 1.5f);
    dl->AddLine(ImVec2(c.x - hs * 0.5f, c.y + hs * 0.55f), ImVec2(c.x + hs * 0.1f, c.y + hs * 0.55f), col, 1.5f);
}

// ── Bordered Row ──────────────────────────────────────────────
static void BeginBorderedRow(ImDrawList* dl, ImVec2 pos, float w, float h)
{
    const ThemePalette& p = ThemeManager::Get().Palette();
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), Anim::ColorU32(p.bgAlt), 10.f);
    dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), Anim::ColorU32(p.border), 10.f, 0, 1.f);
}

// ── Gradient Button ───────────────────────────────────────────
static bool DrawGradientButton(const char* id, const char* label, float w, float h)
{
    const ThemePalette& p = ThemeManager::Get().Palette();
    ImGuiID wid = ImGui::GetID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton(id, ImVec2(w, h));
    bool clicked = ImGui::IsItemClicked();
    bool hov = ImGui::IsItemHovered();

    float t = Anim::Towards(wid, hov ? 1.f : 0.f, 6.f);

    ImVec4 leftCol = Mix(p.btnMasterOff, Brighten(p.btnMasterOff, 1.2f), Anim::Smooth(t));
    ImVec4 rightCol = Mix(p.accent, Brighten(p.accent, 1.15f), Anim::Smooth(t));

    dl->AddRectFilledMultiColor(pos, ImVec2(pos.x + w, pos.y + h),
        Anim::ColorU32(leftCol), Anim::ColorU32(rightCol),
        Anim::ColorU32(rightCol), Anim::ColorU32(leftCol));
    dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h),
        Anim::ColorU32(Dim(p.accent, 0.3f + 0.2f * t)), 10.f, 0, 1.f);

    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(pos.x + (w - ts.x) * 0.5f, pos.y + (h - ts.y) * 0.5f),
        IM_COL32(255, 255, 255, 230), label);

    return clicked;
}

// ── Sidebar (52px, own ImGui window) ──────────────────────────
static void RenderSidebar(float y, float h)
{
    const ThemePalette& p = ThemeManager::Get().Palette();
    const float SW = 52.f;

    ImGui::SetNextWindowPos(ImVec2(0, y));
    ImGui::SetNextWindowSize(ImVec2(SW, h));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, p.bgDeep);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 12));
    ImGui::Begin("##sidebar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor(); ImGui::PopStyleVar(2);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    // Right border
    dl->AddLine(ImVec2(wp.x + SW - 1, wp.y), ImVec2(wp.x + SW - 1, wp.y + h),
        Anim::ColorU32(p.border), 1.f);

    float btnSize = 36.f;
    float bx = (SW - btnSize) * 0.5f;

    struct SideBtn { ActiveTab tab; int iconType; };
    SideBtn btns[] = { {TAB_TRAINER, 0}, {TAB_SETTINGS, 1}, {TAB_LOG, 2} };

    for (int i = 0; i < 3; ++i)
    {
        if (i == 1) {
            // separator
            ImVec2 sepStart = ImGui::GetCursorScreenPos();
            float sepX = sepStart.x + bx + 6.f;
            dl->AddLine(ImVec2(sepX, sepStart.y),
                ImVec2(sepX + btnSize - 12.f, sepStart.y),
                Anim::ColorU32(p.border), 1.f);
            ImGui::Dummy(ImVec2(0, 8));
        }

        bool active = (s_activeTab == btns[i].tab);

        // Position the button
        ImGui::SetCursorPosX(bx);
        char bid[16]; snprintf(bid, sizeof(bid), "##sb%d", i);
        ImGui::InvisibleButton(bid, ImVec2(btnSize, btnSize));
        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) s_activeTab = btns[i].tab;

        ImVec2 bMin = ImGui::GetItemRectMin();
        ImVec2 bMax = ImGui::GetItemRectMax();

        // Draw bg
        if (active) {
            dl->AddRectFilled(bMin, bMax, Anim::ColorU32(Dim(p.accent, 0.08f)), 6.f);
            dl->AddRect(bMin, bMax, Anim::ColorU32(Dim(p.accent, 0.25f)), 6.f, 0, 1.f);
        } else if (hov) {
            dl->AddRectFilled(bMin, bMax, Anim::ColorU32(Dim(p.textPrimary, 0.06f)), 6.f);
        }

        // Draw icon
        ImVec2 ic((bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f);
        ImU32 iconCol = active ? Anim::ColorU32(p.accent) : Anim::ColorU32(p.textDisabled);
        if (btns[i].iconType == 0)      DrawGridIcon(dl, ic, 14.f, iconCol);
        else if (btns[i].iconType == 1) DrawGearIcon(dl, ic, 7.f, iconCol);
        else                             DrawLogIcon(dl, ic, 14.f, iconCol);

        ImGui::Dummy(ImVec2(0, 4));
    }

    ImGui::End();
}

// ── Titlebar (44px, own ImGui window) ─────────────────────────
static void RenderTitleBar(CheatManager& mgr, float w)
{
    const ThemePalette& p = ThemeManager::Get().Palette();
    const float H = 44.f;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(w, H));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, p.bgDeep);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##titlebar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor(); ImGui::PopStyleVar(2);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    // Bottom border
    dl->AddLine(ImVec2(wp.x, wp.y + H - 1), ImVec2(wp.x + w, wp.y + H - 1),
        Anim::ColorU32(Dim(p.accent, 0.6f)), 1.5f);

    // ── Logo bars + text ──────────────────────────────────────
    float lx = wp.x + 14.f, ly = wp.y + (H - 22.f) * 0.5f;
    dl->AddRectFilled(ImVec2(lx, ly + 12.f), ImVec2(lx + 4.f, ly + 22.f),
        Anim::ColorU32(Dim(p.accent, 0.5f)), 1.f);
    dl->AddRectFilled(ImVec2(lx + 6.f, ly + 6.f), ImVec2(lx + 10.f, ly + 22.f),
        Anim::ColorU32(Dim(p.accent, 0.75f)), 1.f);
    dl->AddRectFilled(ImVec2(lx + 12.f, ly), ImVec2(lx + 16.f, ly + 22.f),
        Anim::ColorU32(p.accent), 1.f);
    float titleX = lx + 26.f;
    dl->AddText(ImVec2(titleX, wp.y + (H - ImGui::GetTextLineHeight()) * 0.5f),
        Anim::ColorU32(p.accent), "CT TRAINER");

    // ── Current tab name (center area) ────────────────────────
    float tabsX = titleX + ImGui::CalcTextSize("CT TRAINER").x + 20.f;
    const char* currentTabName = "Trainer";
    if (s_activeTab == TAB_SETTINGS) currentTabName = "Settings";
    else if (s_activeTab == TAB_LOG) currentTabName = "Log";
    dl->AddText(ImVec2(tabsX, wp.y + (H - ImGui::GetTextLineHeight()) * 0.5f),
        Anim::ColorU32(p.textSecondary), currentTabName);
    float afterTabX = tabsX + ImGui::CalcTextSize(currentTabName).x + 10.f;

    // ── Close [x] button ──────────────────────────────────────
    const float BW = 36.f;
    float cx = w - BW;
    ImGui::SetCursorScreenPos(ImVec2(cx, wp.y));
    ImGui::InvisibleButton("##wclose", ImVec2(BW, H));
    bool cHov = ImGui::IsItemHovered();
    dl->AddRectFilled(ImVec2(cx, wp.y), ImVec2(cx + BW, wp.y + H),
        cHov ? IM_COL32(210, 30, 30, 230) : IM_COL32(0, 0, 0, 0));
    { ImVec2 s = ImGui::CalcTextSize("x");
      dl->AddText(ImVec2(cx + (BW - s.x)*0.5f, wp.y + (H - s.y)*0.5f),
        cHov ? IM_COL32(255,255,255,255) : Anim::ColorU32(p.textSecondary), "x"); }
    if (ImGui::IsItemClicked()) PostQuitMessage(0);

    // ── Minimize [_] button ───────────────────────────────────
    float mx = cx - BW;
    ImGui::SetCursorScreenPos(ImVec2(mx, wp.y));
    ImGui::InvisibleButton("##wmini", ImVec2(BW, H));
    bool mHov = ImGui::IsItemHovered();
    dl->AddRectFilled(ImVec2(mx, wp.y), ImVec2(mx + BW, wp.y + H),
        mHov ? Anim::ColorU32(Dim(p.accent, 0.22f)) : IM_COL32(0,0,0,0));
    { ImVec2 s = ImGui::CalcTextSize("_");
      dl->AddText(ImVec2(mx + (BW - s.x)*0.5f, wp.y + (H - s.y)*0.5f - 2.f),
        mHov ? Anim::ColorU32(p.accent) : Anim::ColorU32(p.textSecondary), "_"); }
    if (ImGui::IsItemClicked()) ShowWindow(g_hWnd, SW_MINIMIZE);

    // ── PID / status indicator ────────────────────────────────
    float pulse = (sinf((float)ImGui::GetTime() * 2.f) + 1.f) * 0.5f;
    ImVec4 pidCol = mgr.HProcess() ? p.good : p.bad;
    char pidBuf[32];
    if (mgr.HProcess()) snprintf(pidBuf, sizeof(pidBuf), "PID %lu", (unsigned long)mgr.PID());
    else strncpy_s(pidBuf, sizeof(pidBuf), "DETACHED", _TRUNCATE);
    ImVec2 pidSz = ImGui::CalcTextSize(pidBuf);
    float pidTx = mx - 12.f - pidSz.x;
    float pidDot = pidTx - 12.f;
    dl->AddCircleFilled(ImVec2(pidDot, wp.y + H * 0.5f), 4.f,
        Anim::ColorU32(ImVec4(pidCol.x, pidCol.y, pidCol.z, 0.5f + 0.5f * pulse)));
    dl->AddText(ImVec2(pidTx, wp.y + (H - pidSz.y) * 0.5f),
        Anim::ColorU32(pidCol), pidBuf);

    // ── Drag zone ─────────────────────────────────────────────
    static bool  s_dragActive = false;
    static POINT s_dragOriginCursor = {};
    static POINT s_dragOriginWin    = {};

    float dragW = pidDot - 8.f - afterTabX;
    if (dragW > 0.f) {
        ImGui::SetCursorScreenPos(ImVec2(afterTabX, wp.y));
        ImGui::InvisibleButton("##wdrag", ImVec2(dragW, H));
        if (ImGui::IsItemActivated()) {
            GetCursorPos(&s_dragOriginCursor);
            RECT wr = {}; GetWindowRect(g_hWnd, &wr);
            s_dragOriginWin = { wr.left, wr.top };
            s_dragActive = true;
        }
    }
    if (s_dragActive) {
        if (ImGui::IsMouseDown(0)) {
            POINT cur = {}; GetCursorPos(&cur);
            SetWindowPos(g_hWnd, nullptr,
                s_dragOriginWin.x + (cur.x - s_dragOriginCursor.x),
                s_dragOriginWin.y + (cur.y - s_dragOriginCursor.y),
                0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        } else s_dragActive = false;
    }

    ImGui::End();
}

// ── Custom table header ────────────────────────────────────────
static void DrawTableHeader(float colWidths[4], const char* labels[4], float rowH)
{
    const ThemePalette& p = ThemeManager::Get().Palette();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float  w = ImGui::GetContentRegionAvail().x;

    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + rowH), Anim::ColorU32(p.tableHeader));
    dl->AddLine(ImVec2(pos.x, pos.y + rowH), ImVec2(pos.x + w, pos.y + rowH),
        Anim::ColorU32(p.border), 1.f);

    float x = pos.x + 8.f;
    for (int i = 0; i < 4; ++i)
    {
        ImVec2 tp(x + 2.f, pos.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f);
        dl->AddText(tp, Anim::ColorU32(p.textSecondary), labels[i]);
        if (i < 3) dl->AddLine(ImVec2(x + colWidths[i] - 1, pos.y),
            ImVec2(x + colWidths[i] - 1, pos.y + rowH), Anim::ColorU32(p.border), 1.f);
        x += colWidths[i];
    }
    ImGui::Dummy(ImVec2(w, rowH));
}

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

    while ((int)s_cellStates.size() < (int)mgr.Count())
        s_cellStates.push_back(ValueCell::State{});
    if ((int)s_cellStates.size() > (int)mgr.Count())
        s_cellStates.resize(mgr.Count());

    float totalW = ImGui::GetContentRegionAvail().x;
    float freezeW = 76.f, stateW = 56.f;
    float nameW = totalW * 0.36f;
    float valueW = totalW - nameW - freezeW - stateW;
    float colW[4] = { nameW, valueW, freezeW, stateW };
    const char* hdrs[4] = { "Name", "Value", "Freeze", "State" };
    const float ROW_H = 28.f, HDR_H = 26.f;

    ImGui::BeginChild("##tbl", ImVec2(0, availH), false, ImGuiWindowFlags_NoScrollbar);
    DrawTableHeader(colW, hdrs, HDR_H);
    ImGui::BeginChild("##rows", ImVec2(0, 0), false);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < (int)mgr.Count(); ++i)
    {
        Cheat* c = mgr.GetCheat(i); if (!c) continue;
        bool frozen = c->freezeOn.load();
        ImGui::PushID(i);
        DrawRowBg(i, ROW_H);
        ImVec2 rowStart = ImGui::GetCursorScreenPos();

        float x = rowStart.x + 8.f;
        ImVec4 nameCol = frozen ? p.frozen : p.textPrimary;
        dl->AddText(ImVec2(x, rowStart.y + (ROW_H - ImGui::GetTextLineHeight()) * 0.5f),
            Anim::ColorU32(nameCol), c->description.c_str());

        ImGui::SetCursorScreenPos(ImVec2(rowStart.x + nameW, rowStart.y + 3.f));
        char liveBuf[32]; FormatLive(c, liveBuf, sizeof(liveBuf));
        std::string committed;
        if (ValueCell::Draw(("##vc" + std::to_string(i)).c_str(),
            s_cellStates[i], liveBuf, frozen, valueW - 8.f, committed))
            CommitValue(mgr, i, committed);

        ImGui::SetCursorScreenPos(
            ImVec2(rowStart.x + nameW + valueW + 4.f,
                rowStart.y + (ROW_H - 22.f) * 0.5f));
        if (PillButton::Draw("##fr", "Thaw", "Freeze", frozen,
            ImVec2(freezeW - 8.f, 22.f),
            p.btnThaw, p.btnThawHov, p.btnFreeze, p.btnFreezeHov))
        {
            if (frozen) { mgr.StopFreeze(i); UI::SetStatus(c->description + " unfrozen."); }
            else {
                if (!mgr.HProcess()) UI::SetStatus("Attach first.", true);
                else {
                    if (c->liveReadOk) { c->freezeValueDW = c->liveValueDW; c->freezeValueF = c->liveValueF; }
                    if (!c->resolved) mgr.Resolve(i);
                    if (!c->resolved) UI::SetStatus(c->description + ": unresolved.", true);
                    else { mgr.StartFreeze(i); UI::SetStatus(c->description + " frozen."); }
                }
            }
        }

        ImVec2 statePos(rowStart.x + nameW + valueW + freezeW + 4.f,
            rowStart.y + (ROW_H - ImGui::GetTextLineHeight()) * 0.5f);
        if (!c->resolved)
            dl->AddText(statePos, Anim::ColorU32(p.textDisabled), "  ?");
        else if (frozen) {
            float pls = (sinf((float)ImGui::GetTime() * 2.5f) + 1.f) * 0.5f;
            ImU32 dc = Anim::ColorU32(ImVec4(p.frozen.x, p.frozen.y, p.frozen.z, 0.5f + 0.5f * pls));
            dl->AddCircleFilled(ImVec2(statePos.x + 4.f, statePos.y + ImGui::GetTextLineHeight() * 0.5f), 3.f, dc);
            dl->AddText(ImVec2(statePos.x + 12.f, statePos.y), Anim::ColorU32(p.frozen), "ON");
        } else
            dl->AddText(statePos, Anim::ColorU32(p.textDisabled), "idle");

        dl->AddLine(ImVec2(rowStart.x, rowStart.y + ROW_H),
            ImVec2(rowStart.x + totalW, rowStart.y + ROW_H), Anim::ColorU32(p.border), 1.f);
        ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + ROW_H));
        ImGui::Dummy(ImVec2(totalW, 0.f));
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

// ── Trainer Panel ─────────────────────────────────────────────
static void RenderTrainerPanel(CheatManager& mgr, float panelX, float panelY, float panelW, float panelH)
{
    const ThemePalette& p = ThemeManager::Get().Palette();

    ImGui::SetNextWindowPos(ImVec2(panelX, panelY));
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, p.bg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.f, 16.f));
    ImGui::Begin("##trainer", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor(); ImGui::PopStyleVar(2);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    // ── Attach Row ────────────────────────────────────────────
    {
        ImVec2 rp = ImGui::GetCursorScreenPos();
        float rh = 44.f;
        BeginBorderedRow(dl, rp, w, rh);

        // Plus-circle icon
        float ix = rp.x + 14.f, iy = rp.y + rh * 0.5f;
        dl->AddCircle(ImVec2(ix, iy), 7.f, Anim::ColorU32(p.textDisabled), 0, 1.5f);
        dl->AddLine(ImVec2(ix - 3.f, iy), ImVec2(ix + 3.f, iy), Anim::ColorU32(p.textDisabled), 1.5f);
        dl->AddLine(ImVec2(ix, iy - 3.f), ImVec2(ix, iy + 3.f), Anim::ColorU32(p.textDisabled), 1.5f);

        // Process input
        ImGui::SetCursorScreenPos(ImVec2(rp.x + 30.f, rp.y + 8.f));
        TextInput::Draw("##proc", s_processName, sizeof(s_processName),
            w * 0.3f, "Process name...", p.textSecondary);

        // Divider
        float divX = rp.x + 30.f + w * 0.3f + 10.f;
        dl->AddLine(ImVec2(divX, rp.y + 10.f), ImVec2(divX, rp.y + rh - 10.f),
            Anim::ColorU32(p.border), 1.f);

        // 32-bit toggle label + switch
        float lblY = rp.y + (rh - ImGui::GetTextLineHeight()) * 0.5f;
        dl->AddText(ImVec2(divX + 10.f, lblY), Anim::ColorU32(p.textDisabled), "32-bit");
        ImGui::SetCursorScreenPos(ImVec2(divX + 55.f, rp.y + (rh - 18.f) * 0.5f));
        DrawToggleSwitch("##bittgl", &s_is32Bit);

        // Divider 2
        float div2X = divX + 100.f;
        dl->AddLine(ImVec2(div2X, rp.y + 10.f), ImVec2(div2X, rp.y + rh - 10.f),
            Anim::ColorU32(p.border), 1.f);

        // Attach button
        ImGui::SetCursorScreenPos(ImVec2(div2X + 10.f, rp.y + (rh - 28.f) * 0.5f));
        if (GlowButton::Draw("##att", "Attach", ImVec2(70, 28),
            p.btnAction, p.btnActionHov, p.accent, 6.f))
        {
            mgr.StopAll();
            mgr.SetIs32Bit(s_is32Bit);
            LOG_INFO("--- Attach: '" + std::string(s_processName) + "' (" +
                (s_is32Bit ? "32-bit" : "64-bit") + ") ---");
            std::wstring wps(s_processName, s_processName + strlen(s_processName));
            DWORD pid = Memory::GetProcessID(wps);
            if (!pid) UI::SetStatus("Not found: " + std::string(s_processName), true);
            else {
                HANDLE h = Memory::OpenTarget(pid);
                if (!h) UI::SetStatus("OpenProcess failed - run as Admin!", true);
                else { mgr.SetProcess(h, pid); mgr.ResolveAll();
                       UI::SetStatus("Attached PID " + std::to_string(pid)); }
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(rp.x, rp.y + rh + 10.f));
    }

    // ── File Row ──────────────────────────────────────────────
    {
        ImVec2 rp = ImGui::GetCursorScreenPos();
        float rh = 44.f;
        BeginBorderedRow(dl, rp, w, rh);

        dl->AddText(ImVec2(rp.x + 14.f, rp.y + (rh - ImGui::GetTextLineHeight()) * 0.5f),
            Anim::ColorU32(p.textDisabled), "CT");
        dl->AddLine(ImVec2(rp.x + 38.f, rp.y + 10.f), ImVec2(rp.x + 38.f, rp.y + rh - 10.f),
            Anim::ColorU32(p.border), 1.f);

        ImGui::SetCursorScreenPos(ImVec2(rp.x + 48.f, rp.y + 8.f));
        float pathW = w - 48.f - 160.f;
        TextInput::Draw("##ctp", s_ctFilePath, sizeof(s_ctFilePath),
            pathW, "Path to .CT file...", p.textSecondary);

        ImGui::SetCursorScreenPos(ImVec2(rp.x + 48.f + pathW + 8.f, rp.y + (rh - 28.f) * 0.5f));
        if (GhostButton::Draw("##br", "Browse", ImVec2(64, 28),
            p.textSecondary, Dim(p.accent, 0.12f), 6.f))
            BrowseCT(s_ctFilePath, sizeof(s_ctFilePath));

        ImGui::SameLine(0, 6);
        if (GlowButton::Draw("##lct", "Load CT", ImVec2(70, 28),
            p.btnAction, p.btnActionHov, p.accent, 6.f))
        {
            if (!s_ctFilePath[0]) UI::SetStatus("Select a .CT file.", true);
            else {
                LOG_INFO("--- Load CT: '" + std::string(s_ctFilePath) + "' ---");
                auto entries = CTParser::Load(s_ctFilePath);
                if (entries.empty()) UI::SetStatus("No entries found.", true);
                else {
                    mgr.LoadFromCT(entries); s_cellStates.clear();
                    if (mgr.HProcess()) mgr.ResolveAll();
                    UI::SetStatus("Loaded " + std::to_string(entries.size()) + " entries.");
                }
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(rp.x, rp.y + rh + 10.f));
    }

    // ── Freeze All button (PillButton, same as original) ──────
    {
        bool masterOn = mgr.IsMasterOn();
        float bw = 200.f;
        ImGui::SetCursorPosX((w - bw) * 0.5f);
        if (PillButton::Draw("##mall",
            "DISABLE ALL", "FREEZE ALL",
            masterOn, ImVec2(bw, 32),
            p.btnMasterOn, p.btnMasterOnHov,
            p.btnMasterOff, p.btnMasterOffHov))
        {
            if (!mgr.HProcess()) UI::SetStatus("Attach first.", true);
            else if (!mgr.Count()) UI::SetStatus("Load a CT file first.", true);
            else if (masterOn) { mgr.MasterOff(); UI::SetStatus("All cheats off."); }
            else { mgr.MasterOn(); UI::SetStatus("All frozen!"); }
        }
        ImGui::Dummy(ImVec2(0, 10.f));
    }

    // ── Entries Area ──────────────────────────────────────────
    if (mgr.Count() > 0)
    {
        float used = ImGui::GetCursorPosY();
        float tableH = panelH - used - 8.f;
        RenderCheatTable(mgr, tableH);
    }
    else
    {
        ImVec2 rp = ImGui::GetCursorScreenPos();
        float used = ImGui::GetCursorPosY();
        float rh = panelH - used - 24.f;
        if (rh < 60.f) rh = 60.f;
        BeginBorderedRow(dl, rp, w, rh);

        float cx = rp.x + w * 0.5f, cy = rp.y + rh * 0.4f;
        DrawLogIcon(dl, ImVec2(cx, cy), 28.f, Anim::ColorU32(Dim(p.textDisabled, 0.35f)));
        const char* hint = "Load a .CT file to see entries";
        ImVec2 hs = ImGui::CalcTextSize(hint);
        dl->AddText(ImVec2(cx - hs.x * 0.5f, cy + 22.f), Anim::ColorU32(p.textDisabled), hint);
        ImGui::Dummy(ImVec2(0, rh));
    }

    ImGui::End();
}

// ── Settings Panel ────────────────────────────────────────────
static void RenderSettingsPanel(float panelX, float panelY, float panelW, float panelH)
{
    const ThemePalette& p = ThemeManager::Get().Palette();
    ThemeManager& tm = ThemeManager::Get();
    const auto& allThemes = tm.All();

    ImGui::SetNextWindowPos(ImVec2(panelX, panelY));
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, p.bg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.f, 16.f));
    ImGui::Begin("##settings", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor(); ImGui::PopStyleVar(2);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    auto SectionHdr = [&](const char* title) {
        ImVec2 cp = ImGui::GetCursorScreenPos();
        dl->AddText(cp, Anim::ColorU32(p.accent), title);
        ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight()));
        Divider(2, 8);
    };

    // ─── THEME (Custom Dropdown) ────────────────────────────────
    SectionHdr("THEME");
    {
        static bool s_themeOpen = false;
        float ddW = fminf(320.f, w);
        ImVec2 trigPos = ImGui::GetCursorScreenPos();
        float trigH = 36.f;

        // Trigger button
        ImGui::InvisibleButton("##theme_trig", ImVec2(ddW, trigH));
        bool trigHov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) s_themeOpen = !s_themeOpen;

        ImVec2 tMin = ImGui::GetItemRectMin();
        ImVec2 tMax = ImGui::GetItemRectMax();

        // Trigger background
        float trigR = s_themeOpen ? 0.f : 10.f;
        dl->AddRectFilled(tMin, tMax, Anim::ColorU32(p.bgAlt),
            10.f, s_themeOpen ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll);
        dl->AddRect(tMin, tMax, Anim::ColorU32(s_themeOpen ? Dim(p.accent, 0.35f) : p.border),
            10.f, s_themeOpen ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll, 1.f);

        // Selected text
        dl->AddText(ImVec2(tMin.x + 14.f, tMin.y + (trigH - ImGui::GetTextLineHeight()) * 0.5f),
            Anim::ColorU32(p.textPrimary), allThemes[tm.Current()].name);

        // Arrow
        float arX = tMax.x - 20.f, arY = tMin.y + trigH * 0.5f;
        if (s_themeOpen) {
            dl->AddTriangleFilled(ImVec2(arX - 4.f, arY + 2.f), ImVec2(arX + 4.f, arY + 2.f),
                ImVec2(arX, arY - 3.f), Anim::ColorU32(p.textDisabled));
        } else {
            dl->AddTriangleFilled(ImVec2(arX - 4.f, arY - 2.f), ImVec2(arX + 4.f, arY - 2.f),
                ImVec2(arX, arY + 3.f), Anim::ColorU32(p.textDisabled));
        }

        // Dropdown list
        if (s_themeOpen) {
            for (int i = 0; i < (int)allThemes.size(); ++i) {
                ImVec2 optPos = ImGui::GetCursorScreenPos();
                float optH = 34.f;
                bool isLast = (i == (int)allThemes.size() - 1);
                bool active = (tm.Current() == i);

                ImGui::InvisibleButton(("##topt" + std::to_string(i)).c_str(), ImVec2(ddW, optH));
                bool optHov = ImGui::IsItemHovered();

                ImVec2 oMin = ImGui::GetItemRectMin();
                ImVec2 oMax = ImGui::GetItemRectMax();

                ImDrawFlags rflags = isLast ? ImDrawFlags_RoundCornersBottom : ImDrawFlags_RoundCornersNone;
                ImVec4 optBg = active ? Dim(p.accent, 0.08f) : (optHov ? p.bgAlt : p.inputBg);
                dl->AddRectFilled(oMin, oMax, Anim::ColorU32(optBg), isLast ? 10.f : 0.f, rflags);
                dl->AddLine(ImVec2(oMin.x, oMin.y), ImVec2(oMax.x, oMin.y),
                    Anim::ColorU32(Dim(p.accent, 0.25f)), 0.5f);
                if (isLast) {
                    dl->AddRect(ImVec2(oMin.x, oMin.y), oMax,
                        Anim::ColorU32(Dim(p.accent, 0.25f)), 10.f, ImDrawFlags_RoundCornersBottom, 1.f);
                } else {
                    dl->AddLine(ImVec2(oMin.x, oMin.y), ImVec2(oMin.x, oMax.y), Anim::ColorU32(Dim(p.accent, 0.25f)), 1.f);
                    dl->AddLine(ImVec2(oMax.x, oMin.y), ImVec2(oMax.x, oMax.y), Anim::ColorU32(Dim(p.accent, 0.25f)), 1.f);
                }

                ImVec4 txtCol = active ? p.accent : (optHov ? p.textPrimary : p.textSecondary);
                dl->AddText(ImVec2(oMin.x + 14.f, oMin.y + (optH - ImGui::GetTextLineHeight()) * 0.5f),
                    Anim::ColorU32(txtCol), allThemes[i].name);

                // Active dot
                if (active)
                    dl->AddCircleFilled(ImVec2(oMax.x - 14.f, oMin.y + optH * 0.5f), 3.f, Anim::ColorU32(p.accent));

                if (ImGui::IsItemClicked()) {
                    tm.Apply(i);
                    s_themeOpen = false;
                }
            }
        }
    }
    ImGui::Dummy(ImVec2(0, 12));

    // ─── FONT (Custom Dropdown) ───────────────────────────────
    SectionHdr("FONT");
    {
        static bool s_fontOpen = false;
        float ddW = fminf(320.f, w);
        ImVec2 trigPos = ImGui::GetCursorScreenPos();
        float trigH = 36.f;

        ImGui::InvisibleButton("##font_trig", ImVec2(ddW, trigH));
        bool trigHov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) s_fontOpen = !s_fontOpen;

        ImVec2 tMin = ImGui::GetItemRectMin();
        ImVec2 tMax = ImGui::GetItemRectMax();

        dl->AddRectFilled(tMin, tMax, Anim::ColorU32(p.bgAlt),
            10.f, s_fontOpen ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll);
        dl->AddRect(tMin, tMax, Anim::ColorU32(s_fontOpen ? Dim(p.accent, 0.35f) : p.border),
            10.f, s_fontOpen ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll, 1.f);

        dl->AddText(ImVec2(tMin.x + 14.f, tMin.y + (trigH - ImGui::GetTextLineHeight()) * 0.5f),
            Anim::ColorU32(p.textPrimary), k_UIFontNames[s_fontIndex]);

        float arX = tMax.x - 20.f, arY = tMin.y + trigH * 0.5f;
        if (s_fontOpen) {
            dl->AddTriangleFilled(ImVec2(arX - 4.f, arY + 2.f), ImVec2(arX + 4.f, arY + 2.f),
                ImVec2(arX, arY - 3.f), Anim::ColorU32(p.textDisabled));
        } else {
            dl->AddTriangleFilled(ImVec2(arX - 4.f, arY - 2.f), ImVec2(arX + 4.f, arY - 2.f),
                ImVec2(arX, arY + 3.f), Anim::ColorU32(p.textDisabled));
        }

        if (s_fontOpen) {
            for (int i = 0; i < k_UIFontCount; ++i) {
                float optH = 34.f;
                bool isLast = (i == k_UIFontCount - 1);
                bool active = (s_fontIndex == i);
                bool avail = (g_Fonts[i] != nullptr);

                ImGui::InvisibleButton(("##fopt" + std::to_string(i)).c_str(), ImVec2(ddW, optH));
                bool optHov = ImGui::IsItemHovered();

                ImVec2 oMin = ImGui::GetItemRectMin();
                ImVec2 oMax = ImGui::GetItemRectMax();

                ImDrawFlags rflags = isLast ? ImDrawFlags_RoundCornersBottom : ImDrawFlags_RoundCornersNone;
                ImVec4 optBg = active ? Dim(p.accent, 0.08f) : (optHov ? p.bgAlt : p.inputBg);
                dl->AddRectFilled(oMin, oMax, Anim::ColorU32(optBg), isLast ? 10.f : 0.f, rflags);
                dl->AddLine(ImVec2(oMin.x, oMin.y), ImVec2(oMax.x, oMin.y),
                    Anim::ColorU32(Dim(p.accent, 0.25f)), 0.5f);
                if (isLast) {
                    dl->AddRect(ImVec2(oMin.x, oMin.y), oMax,
                        Anim::ColorU32(Dim(p.accent, 0.25f)), 10.f, ImDrawFlags_RoundCornersBottom, 1.f);
                } else {
                    dl->AddLine(ImVec2(oMin.x, oMin.y), ImVec2(oMin.x, oMax.y), Anim::ColorU32(Dim(p.accent, 0.25f)), 1.f);
                    dl->AddLine(ImVec2(oMax.x, oMin.y), ImVec2(oMax.x, oMax.y), Anim::ColorU32(Dim(p.accent, 0.25f)), 1.f);
                }

                ImVec4 txtCol = active ? p.accent : avail ? (optHov ? p.textPrimary : p.textSecondary) : p.textDisabled;
                dl->AddText(ImVec2(oMin.x + 14.f, oMin.y + (optH - ImGui::GetTextLineHeight()) * 0.5f),
                    Anim::ColorU32(txtCol), k_UIFontNames[i]);

                if (active)
                    dl->AddCircleFilled(ImVec2(oMax.x - 14.f, oMin.y + optH * 0.5f), 3.f, Anim::ColorU32(p.accent));

                if (ImGui::IsItemClicked() && avail && i != s_fontIndex) {
                    s_fontIndex = i;
                    ImGui::GetIO().FontDefault = g_Fonts[i];
                    RebuildFontAtlas();
                    s_fontOpen = false;
                }
            }
        }
    }
    ImGui::Dummy(ImVec2(0, 12));

    // ─── OPACITY ──────────────────────────────────────────────
    SectionHdr("OPACITY");
    {
        int alpha = s_alpha;
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, p.accent);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, p.accentHover);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, p.inputBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, p.accentDim);
        ImGui::SetNextItemWidth(w - 60.f);
        if (ImGui::SliderInt("##opacity", &alpha, 77, 255, ""))
        { s_alpha = alpha; SetWindowAlpha(s_alpha); }
        ImGui::PopStyleColor(4);
        ImGui::SameLine(0, 12);
        char pct[16]; snprintf(pct, sizeof(pct), "%d%%", (int)((alpha / 255.f) * 100.f));
        ImVec2 pp = ImGui::GetCursorScreenPos();
        dl->AddText(ImVec2(pp.x, pp.y + 3.f), Anim::ColorU32(p.textSecondary), pct);
        ImGui::Dummy(ImVec2(0, 22));
    }
    ImGui::Dummy(ImVec2(0, 4));

    // ─── WINDOW EFFECTS (Toggle Switches) ─────────────────────
    SectionHdr("WINDOW EFFECTS");
    {
        // Blur toggle with inline switch
        ImVec2 bfPos = ImGui::GetCursorScreenPos();
        float fxW = 120.f, fxH = 36.f;

        // Blur button
        ImGui::InvisibleButton("##blurfx", ImVec2(fxW, fxH));
        bool bHov = ImGui::IsItemHovered();
        ImVec2 bMin = ImGui::GetItemRectMin(), bMax = ImGui::GetItemRectMax();
        dl->AddRectFilled(bMin, bMax, Anim::ColorU32(p.bgAlt), 6.f);
        dl->AddRect(bMin, bMax, Anim::ColorU32(p.border), 6.f, 0, 1.f);

        // Draw mini toggle inside
        {
            float tw = 28.f, th = 14.f;
            float tx = bMin.x + 10.f, ty = bMin.y + (fxH - th) * 0.5f;
            ImGuiID twid = ImGui::GetID("##blurfxtgl");
            float t = Anim::Towards(twid, s_blurEnabled ? 1.f : 0.f, 8.f);

            dl->AddRectFilled(ImVec2(tx, ty), ImVec2(tx + tw, ty + th),
                Anim::ColorU32(Mix(p.inputBg, Dim(p.accent, 0.25f), t)), 99.f);
            dl->AddRect(ImVec2(tx, ty), ImVec2(tx + tw, ty + th),
                Anim::ColorU32(Mix(p.border, Dim(p.accent, 0.6f), t)), 99.f, 0, 1.f);
            float kd = 10.f, kx = tx + 2.f + t * (tw - kd - 4.f);
            dl->AddCircleFilled(ImVec2(kx + kd * 0.5f, ty + th * 0.5f), kd * 0.5f,
                Anim::ColorU32(Mix(p.textDisabled, p.accent, t)));

            dl->AddText(ImVec2(tx + tw + 8.f, bMin.y + (fxH - ImGui::GetTextLineHeight()) * 0.5f),
                Anim::ColorU32(p.textSecondary), "Blur");
        }
        if (ImGui::IsItemClicked()) {
            s_blurEnabled = !s_blurEnabled;
            if (s_blurEnabled) { s_acrylicEnabled = false; SetWindowAcrylic(false); }
            SetWindowBlur(s_blurEnabled);
        }

        ImGui::SameLine(0, 8);

        // Acrylic button
        ImGui::InvisibleButton("##acrylicfx", ImVec2(fxW + 10.f, fxH));
        bool aHov = ImGui::IsItemHovered();
        ImVec2 aMin = ImGui::GetItemRectMin(), aMax = ImGui::GetItemRectMax();
        dl->AddRectFilled(aMin, aMax, Anim::ColorU32(p.bgAlt), 6.f);
        dl->AddRect(aMin, aMax, Anim::ColorU32(p.border), 6.f, 0, 1.f);

        // Draw mini toggle inside
        {
            float tw = 28.f, th = 14.f;
            float tx = aMin.x + 10.f, ty = aMin.y + (fxH - th) * 0.5f;
            ImGuiID twid = ImGui::GetID("##acrylicfxtgl");
            float t = Anim::Towards(twid, s_acrylicEnabled ? 1.f : 0.f, 8.f);

            dl->AddRectFilled(ImVec2(tx, ty), ImVec2(tx + tw, ty + th),
                Anim::ColorU32(Mix(p.inputBg, Dim(p.accent, 0.25f), t)), 99.f);
            dl->AddRect(ImVec2(tx, ty), ImVec2(tx + tw, ty + th),
                Anim::ColorU32(Mix(p.border, Dim(p.accent, 0.6f), t)), 99.f, 0, 1.f);
            float kd = 10.f, kx = tx + 2.f + t * (tw - kd - 4.f);
            dl->AddCircleFilled(ImVec2(kx + kd * 0.5f, ty + th * 0.5f), kd * 0.5f,
                Anim::ColorU32(Mix(p.textDisabled, p.accent, t)));

            dl->AddText(ImVec2(tx + tw + 8.f, aMin.y + (fxH - ImGui::GetTextLineHeight()) * 0.5f),
                Anim::ColorU32(p.textSecondary), "Acrylic");
        }
        if (ImGui::IsItemClicked()) {
            s_acrylicEnabled = !s_acrylicEnabled;
            if (s_acrylicEnabled) { s_blurEnabled = false; SetWindowBlur(false); }
            SetWindowAcrylic(s_acrylicEnabled);
        }

        if (s_blurEnabled || s_acrylicEnabled) {
            ImGui::Dummy(ImVec2(0, 6));
            ImVec2 tp = ImGui::GetCursorScreenPos();
            dl->AddText(tp, Anim::ColorU32(p.warn),
                "Tip: lower Opacity to see the effect behind the window.");
            ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight()));
        }
    }

    ImGui::End();
}

// ── Log Panel ─────────────────────────────────────────────────
static void RenderLogPanel(float panelX, float panelY, float panelW, float panelH)
{
    const ThemePalette& p = ThemeManager::Get().Palette();

    ImGui::SetNextWindowPos(ImVec2(panelX, panelY));
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, p.bg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.f, 16.f));
    ImGui::Begin("##logpanel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor(); ImGui::PopStyleVar(2);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    // Log area with bordered background
    ImVec2 lp = ImGui::GetCursorScreenPos();
    float lh = panelH - 32.f;
    if (lh < 40.f) lh = 40.f;
    BeginBorderedRow(dl, lp, w, lh);

    ImGui::SetCursorScreenPos(ImVec2(lp.x + 14.f, lp.y + 10.f));
    ImGui::BeginChild("##logscroll", ImVec2(w - 28.f, lh - 20.f), false);

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
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopStyleColor();
    }
    if (s_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f)
        ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();

    ImGui::End();
}

// ── Main render ───────────────────────────────────────────────
void UI::Render(CheatManager& mgr)
{
    ImGuiIO& io = ImGui::GetIO();
    float W = io.DisplaySize.x;
    float H = io.DisplaySize.y;

    const float TITLEBAR_H = 44.f;
    const float SIDEBAR_W  = 52.f;
    const float STATUSBAR_H = 28.f;
    float panelX = SIDEBAR_W;
    float panelY = TITLEBAR_H;
    float panelW = W - SIDEBAR_W;
    float panelH = H - TITLEBAR_H - STATUSBAR_H;

    // Render in correct z-order
    // 1. Active panel first (background)
    switch (s_activeTab)
    {
    case TAB_TRAINER:  RenderTrainerPanel(mgr, panelX, panelY, panelW, panelH); break;
    case TAB_SETTINGS: RenderSettingsPanel(panelX, panelY, panelW, panelH); break;
    case TAB_LOG:      RenderLogPanel(panelX, panelY, panelW, panelH); break;
    }

    // 2. Sidebar
    RenderSidebar(TITLEBAR_H, H - TITLEBAR_H - STATUSBAR_H);

    // 3. Titlebar (on top)
    RenderTitleBar(mgr, W);

    // 4. Status bar (always on top)
    StatusBar::Draw(H - STATUSBAR_H, W, STATUSBAR_H,
        s_statusMsg.c_str(), s_statusError);
}
