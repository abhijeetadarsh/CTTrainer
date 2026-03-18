#pragma once
// ============================================================
//  Theme.h  --  ImGui Theme Manager
//  5 handcrafted themes, switchable at runtime
// ============================================================
#include "imgui.h"
#include <string>
#include <vector>

// ── Per-theme color palette ──────────────────────────────────
struct ThemePalette
{
    const char* name;

    // Window & background
    ImVec4 bg;            // main window bg
    ImVec4 bgAlt;         // alternate row / panel bg
    ImVec4 bgDeep;        // deepest bg (status bar, header)
    ImVec4 border;        // separator / border

    // Text
    ImVec4 textPrimary;
    ImVec4 textSecondary; // labels, hints
    ImVec4 textDisabled;

    // Accent (buttons, highlights)
    ImVec4 accent;
    ImVec4 accentHover;
    ImVec4 accentDim;     // subtle accent (selected rows, etc.)

    // Semantic colours
    ImVec4 good;          // resolved, frozen, success
    ImVec4 warn;          // warnings
    ImVec4 bad;           // error, unresolved
    ImVec4 frozen;        // value colour when frozen

    // Input fields
    ImVec4 inputBg;
    ImVec4 inputBgEdit;   // input bg while editing

    // Buttons
    ImVec4 btnFreeze;     ImVec4 btnFreezeHov;
    ImVec4 btnThaw;       ImVec4 btnThawHov;
    ImVec4 btnAction;     ImVec4 btnActionHov;
    ImVec4 btnMasterOn;   ImVec4 btnMasterOnHov;
    ImVec4 btnMasterOff;  ImVec4 btnMasterOffHov;

    // Table header
    ImVec4 tableHeader;
    ImVec4 tableRowEven;
    ImVec4 tableRowOdd;

    // Rounding & misc
    float  windowRounding;
    float  frameRounding;
    float  itemSpacingY;
};

class ThemeManager
{
public:
    static ThemeManager& Get();

    void Apply(int index);          // apply palette[index] to ImGui
    void ApplyCurrent();

    int                         Current()  const { return m_current; }
    const ThemePalette& Palette()  const { return m_palettes[m_current]; }
    const std::vector<ThemePalette>& All() const { return m_palettes; }

    // Handy accessors used by UI.cpp
    ImVec4 Bg()           const { return m_palettes[m_current].bg; }
    ImVec4 Accent()       const { return m_palettes[m_current].accent; }
    ImVec4 Good()         const { return m_palettes[m_current].good; }
    ImVec4 Bad()          const { return m_palettes[m_current].bad; }
    ImVec4 Frozen()       const { return m_palettes[m_current].frozen; }
    ImVec4 TextPrimary()  const { return m_palettes[m_current].textPrimary; }
    ImVec4 TextSecondary()const { return m_palettes[m_current].textSecondary; }

private:
    ThemeManager();
    int                    m_current = 0;
    std::vector<ThemePalette> m_palettes;
};