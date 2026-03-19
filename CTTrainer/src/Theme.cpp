// ============================================================
//  Theme.cpp  --  5 handcrafted ImGui themes
//
//  MIDNIGHT SYNTHWAVE  -- deep navy + electric magenta/cyan
//  OBSIDIAN BLADE      -- pure black + blood orange + gold
//  ARCTIC TERMINAL     -- near-white + ice blue + steel
//  MILITARY OPS        -- dark olive + amber + khaki
//  NEON NOIR           -- charcoal + toxic green + purple
// ============================================================
#include "Theme.h"

// Helper: build RGBA from 0-255 values
static ImVec4 C(int r, int g, int b, int a = 255)
{
    return ImVec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
}

ThemeManager::ThemeManager()
{
    // ── 0: MIDNIGHT SYNTHWAVE ────────────────────────────────
    {
        ThemePalette p;
        p.name = "Midnight Synthwave";
        p.bg = C(10, 10, 22);
        p.bgAlt = C(14, 14, 30);
        p.bgDeep = C(6, 6, 14);
        p.border = C(50, 30, 80);
        p.textPrimary = C(220, 210, 255);
        p.textSecondary = C(140, 120, 190);
        p.textDisabled = C(70, 60, 100);
        p.accent = C(180, 40, 255);
        p.accentHover = C(200, 80, 255);
        p.accentDim = C(60, 10, 90);
        p.good = C(40, 230, 180);
        p.warn = C(255, 200, 0);
        p.bad = C(255, 60, 100);
        p.frozen = C(0, 255, 200);
        p.inputBg = C(18, 14, 35);
        p.inputBgEdit = C(20, 10, 50);
        p.btnFreeze = C(20, 120, 80);  p.btnFreezeHov = C(30, 160, 100);
        p.btnThaw = C(120, 20, 60);  p.btnThawHov = C(160, 30, 80);
        p.btnAction = C(60, 20, 140);  p.btnActionHov = C(90, 40, 180);
        p.btnMasterOff = C(80, 10, 160);  p.btnMasterOffHov = C(110, 20, 200);
        p.btnMasterOn = C(140, 10, 60);  p.btnMasterOnHov = C(180, 20, 80);
        p.tableHeader = C(20, 12, 45);
        p.tableRowEven = C(10, 10, 22);
        p.tableRowOdd = C(14, 12, 28);
        p.windowRounding = 0.f;
        p.frameRounding = 0.f;
        p.itemSpacingY = 6.f;
        m_palettes.push_back(p);
    }

    // ── 1: OBSIDIAN BLADE ────────────────────────────────────
    {
        ThemePalette p;
        p.name = "Obsidian Blade";
        p.bg = C(8, 8, 8);
        p.bgAlt = C(12, 12, 12);
        p.bgDeep = C(4, 4, 4);
        p.border = C(50, 35, 0);
        p.textPrimary = C(240, 220, 180);
        p.textSecondary = C(160, 130, 80);
        p.textDisabled = C(70, 60, 40);
        p.accent = C(220, 130, 0);
        p.accentHover = C(255, 160, 20);
        p.accentDim = C(50, 30, 0);
        p.good = C(180, 220, 80);
        p.warn = C(255, 160, 0);
        p.bad = C(220, 50, 30);
        p.frozen = C(180, 220, 80);
        p.inputBg = C(15, 12, 5);
        p.inputBgEdit = C(25, 18, 5);
        p.btnFreeze = C(60, 80, 0);  p.btnFreezeHov = C(90, 115, 0);
        p.btnThaw = C(100, 20, 10);  p.btnThawHov = C(140, 30, 15);
        p.btnAction = C(80, 55, 0);  p.btnActionHov = C(120, 80, 0);
        p.btnMasterOff = C(90, 60, 0);  p.btnMasterOffHov = C(130, 90, 0);
        p.btnMasterOn = C(110, 18, 8);  p.btnMasterOnHov = C(150, 26, 12);
        p.tableHeader = C(18, 14, 4);
        p.tableRowEven = C(8, 8, 8);
        p.tableRowOdd = C(13, 11, 5);
        p.windowRounding = 0.f;
        p.frameRounding = 0.f;
        p.itemSpacingY = 5.f;
        m_palettes.push_back(p);
    }

    // ── 2: ARCTIC TERMINAL ───────────────────────────────────
    {
        ThemePalette p;
        p.name = "Arctic Terminal";
        p.bg = C(230, 235, 245);
        p.bgAlt = C(218, 225, 238);
        p.bgDeep = C(200, 210, 228);
        p.border = C(170, 185, 210);
        p.textPrimary = C(20, 30, 60);
        p.textSecondary = C(70, 90, 140);
        p.textDisabled = C(150, 165, 195);
        p.accent = C(30, 100, 220);
        p.accentHover = C(50, 130, 255);
        p.accentDim = C(180, 200, 240);
        p.good = C(0, 140, 80);
        p.warn = C(200, 120, 0);
        p.bad = C(200, 30, 50);
        p.frozen = C(0, 140, 200);
        p.inputBg = C(210, 218, 235);
        p.inputBgEdit = C(190, 205, 235);
        p.btnFreeze = C(20, 90, 50);  p.btnFreezeHov = C(30, 120, 70);
        p.btnThaw = C(160, 20, 35);  p.btnThawHov = C(200, 30, 45);
        p.btnAction = C(20, 80, 200);  p.btnActionHov = C(30, 110, 240);
        p.btnMasterOff = C(20, 80, 180);  p.btnMasterOffHov = C(30, 110, 220);
        p.btnMasterOn = C(160, 20, 35);  p.btnMasterOnHov = C(200, 30, 45);
        p.tableHeader = C(200, 210, 230);
        p.tableRowEven = C(230, 235, 245);
        p.tableRowOdd = C(220, 227, 240);
        p.windowRounding = 0.f;
        p.frameRounding = 0.f;
        p.itemSpacingY = 6.f;
        m_palettes.push_back(p);
    }

    // ── 3: MILITARY OPS ──────────────────────────────────────
    {
        ThemePalette p;
        p.name = "Military Ops";
        p.bg = C(18, 20, 14);
        p.bgAlt = C(22, 25, 17);
        p.bgDeep = C(12, 13, 9);
        p.border = C(50, 55, 30);
        p.textPrimary = C(195, 200, 160);
        p.textSecondary = C(130, 140, 90);
        p.textDisabled = C(70, 75, 50);
        p.accent = C(180, 150, 30);
        p.accentHover = C(220, 185, 50);
        p.accentDim = C(50, 45, 8);
        p.good = C(100, 200, 60);
        p.warn = C(220, 160, 20);
        p.bad = C(220, 60, 40);
        p.frozen = C(100, 200, 60);
        p.inputBg = C(22, 25, 16);
        p.inputBgEdit = C(28, 34, 18);
        p.btnFreeze = C(40, 70, 15);  p.btnFreezeHov = C(55, 95, 20);
        p.btnThaw = C(80, 25, 10);  p.btnThawHov = C(110, 35, 15);
        p.btnAction = C(60, 55, 10);  p.btnActionHov = C(85, 78, 15);
        p.btnMasterOff = C(55, 50, 8);  p.btnMasterOffHov = C(80, 72, 12);
        p.btnMasterOn = C(85, 22, 10);  p.btnMasterOnHov = C(115, 30, 14);
        p.tableHeader = C(25, 28, 18);
        p.tableRowEven = C(18, 20, 14);
        p.tableRowOdd = C(22, 25, 17);
        p.windowRounding = 0.f;
        p.frameRounding = 0.f;
        p.itemSpacingY = 5.f;
        m_palettes.push_back(p);
    }

    // ── 4: NEON NOIR ─────────────────────────────────────────
    {
        ThemePalette p;
        p.name = "Neon Noir";
        p.bg = C(13, 13, 17);
        p.bgAlt = C(17, 17, 22);
        p.bgDeep = C(8, 8, 11);
        p.border = C(30, 80, 30);
        p.textPrimary = C(200, 255, 200);
        p.textSecondary = C(100, 180, 100);
        p.textDisabled = C(50, 80, 50);
        p.accent = C(30, 255, 80);
        p.accentHover = C(80, 255, 120);
        p.accentDim = C(10, 50, 15);
        p.good = C(30, 255, 80);
        p.warn = C(255, 200, 0);
        p.bad = C(255, 50, 80);
        p.frozen = C(30, 255, 80);
        p.inputBg = C(15, 20, 15);
        p.inputBgEdit = C(10, 30, 10);
        p.btnFreeze = C(8, 80, 20);  p.btnFreezeHov = C(12, 110, 28);
        p.btnThaw = C(90, 10, 25);  p.btnThawHov = C(125, 15, 35);
        p.btnAction = C(10, 60, 50);  p.btnActionHov = C(14, 90, 70);
        p.btnMasterOff = C(8, 70, 18);  p.btnMasterOffHov = C(12, 100, 26);
        p.btnMasterOn = C(90, 10, 25);  p.btnMasterOnHov = C(125, 15, 35);
        p.tableHeader = C(10, 22, 10);
        p.tableRowEven = C(13, 13, 17);
        p.tableRowOdd = C(16, 18, 16);
        p.windowRounding = 0.f;
        p.frameRounding = 0.f;
        p.itemSpacingY = 6.f;
        m_palettes.push_back(p);
    }

    Apply(0);
}

ThemeManager& ThemeManager::Get()
{
    static ThemeManager instance;
    return instance;
}

void ThemeManager::Apply(int index)
{
    if (index < 0 || index >= (int)m_palettes.size()) return;
    m_current = index;
    ApplyCurrent();
}

void ThemeManager::ApplyCurrent()
{
    const ThemePalette& p = m_palettes[m_current];
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;

    s.WindowRounding = p.windowRounding;
    s.FrameRounding = p.frameRounding;
    s.GrabRounding = p.frameRounding;
    s.ScrollbarRounding = p.frameRounding;
    s.TabRounding = p.frameRounding;
    s.PopupRounding = p.windowRounding;
    s.ChildRounding = p.windowRounding;
    s.WindowBorderSize = 1.f;
    s.FrameBorderSize = 1.f;
    s.ItemSpacing = ImVec2(8.f, p.itemSpacingY);
    s.FramePadding = ImVec2(8.f, 4.f);
    s.WindowPadding = ImVec2(12.f, 10.f);
    s.ScrollbarSize = 10.f;
    s.GrabMinSize = 8.f;

    c[ImGuiCol_WindowBg] = p.bg;
    c[ImGuiCol_ChildBg] = p.bgAlt;
    c[ImGuiCol_PopupBg] = p.bgAlt;
    c[ImGuiCol_Border] = p.border;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = p.inputBg;
    c[ImGuiCol_FrameBgHovered] = p.accentDim;
    c[ImGuiCol_FrameBgActive] = p.inputBgEdit;
    c[ImGuiCol_TitleBg] = p.bgDeep;
    c[ImGuiCol_TitleBgActive] = p.bgDeep;
    c[ImGuiCol_TitleBgCollapsed] = p.bgDeep;
    c[ImGuiCol_MenuBarBg] = p.bgDeep;
    c[ImGuiCol_ScrollbarBg] = p.bgDeep;
    c[ImGuiCol_ScrollbarGrab] = p.border;
    c[ImGuiCol_ScrollbarGrabHovered] = p.textDisabled;
    c[ImGuiCol_ScrollbarGrabActive] = p.textSecondary;
    c[ImGuiCol_CheckMark] = p.accent;
    c[ImGuiCol_SliderGrab] = p.accent;
    c[ImGuiCol_SliderGrabActive] = p.accentHover;
    c[ImGuiCol_Button] = p.accentDim;
    c[ImGuiCol_ButtonHovered] = p.accent;
    c[ImGuiCol_ButtonActive] = p.accentHover;
    c[ImGuiCol_Header] = p.accentDim;
    c[ImGuiCol_HeaderHovered] = p.accentDim;
    c[ImGuiCol_HeaderActive] = p.accentDim;
    c[ImGuiCol_Separator] = p.border;
    c[ImGuiCol_SeparatorHovered] = p.accent;
    c[ImGuiCol_SeparatorActive] = p.accentHover;
    c[ImGuiCol_ResizeGrip] = p.border;
    c[ImGuiCol_ResizeGripHovered] = p.accent;
    c[ImGuiCol_ResizeGripActive] = p.accentHover;
    c[ImGuiCol_Tab] = p.bgAlt;
    c[ImGuiCol_TabHovered] = p.accentDim;
    c[ImGuiCol_TabActive] = p.accentDim;
    c[ImGuiCol_TabUnfocused] = p.bgDeep;
    c[ImGuiCol_TabUnfocusedActive] = p.bgAlt;
    c[ImGuiCol_TableHeaderBg] = p.tableHeader;
    c[ImGuiCol_TableBorderStrong] = p.border;
    c[ImGuiCol_TableBorderLight] = p.border;
    c[ImGuiCol_TableRowBg] = p.tableRowEven;
    c[ImGuiCol_TableRowBgAlt] = p.tableRowOdd;
    c[ImGuiCol_TextSelectedBg] = p.accentDim;
    c[ImGuiCol_NavHighlight] = p.accent;
    c[ImGuiCol_Text] = p.textPrimary;
    c[ImGuiCol_TextDisabled] = p.textDisabled;
}