#pragma once
// ============================================================
//  Widgets.h  --  Custom ImGui widget library
// ...
// ============================================================
#define IMGUI_DEFINE_MATH_OPERATORS   // ImVec2 +, -, *, / operators
#include "imgui.h"
#include "imgui_internal.h"
#include "Theme.h"
#include <string>
#include <functional>

// ── Easing & animation helpers ───────────────────────────────
namespace Anim
{
    // Returns 0..1 smoothstep of a 0..1 input
    inline float Smooth(float t)
    {
        return t * t * (3.f - 2.f * t);
    }

    // Per-ID float tracker: call every frame, returns animated value
    // speed = units per second towards target
    float Towards(ImGuiID id, float target, float speed);

    // Lerp two colors
    inline ImVec4 LerpColor(ImVec4 a, ImVec4 b, float t)
    {
        return ImVec4(
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t);
    }

    inline ImU32 ColorU32(ImVec4 c)
    {
        return ImGui::ColorConvertFloat4ToU32(c);
    }
}

// ── Base widget ──────────────────────────────────────────────
class Widget
{
public:
    virtual ~Widget() = default;

protected:
    // Reserve a rect in the ImGui layout and return it
    static ImRect Reserve(float w, float h);

    // Check hover / click against a rect
    static bool IsHovered(const ImRect& r);
    static bool IsClicked(const ImRect& r, int btn = 0);
    static bool IsDblClicked(const ImRect& r);

    // Shared draw helpers
    static void DrawRoundRect(ImDrawList* dl, ImRect r, ImU32 col,
        float rounding, float thickness = 0.f);
    static void DrawGlow(ImDrawList* dl, ImRect r, ImU32 col,
        float rounding, float spread);
    static void DrawCenteredText(ImDrawList* dl, ImRect r,
        ImU32 col, const char* text);
};

// ── GlowButton ───────────────────────────────────────────────
// Solid fill + accent glow ring on hover, shrinks on click
class GlowButton : public Widget
{
public:
    // Returns true if clicked
    static bool Draw(const char* id, const char* label,
        ImVec2 size,
        ImVec4 colFill, ImVec4 colFillHov,
        ImVec4 colGlow,
        float rounding = 5.f);
};

// ── GhostButton ──────────────────────────────────────────────
// Transparent bg, border outline, fills on hover
class GhostButton : public Widget
{
public:
    static bool Draw(const char* id, const char* label,
        ImVec2 size,
        ImVec4 colBorder, ImVec4 colHovFill,
        float rounding = 5.f);
};

// ── PillButton ────────────────────────────────────────────────
// Fully rounded, two-state toggle
class PillButton : public Widget
{
public:
    // active=true shows "on" style; returns true if state toggled
    static bool Draw(const char* id, const char* labelOn,
        const char* labelOff, bool active,
        ImVec2 size,
        ImVec4 colOn, ImVec4 colOnHov,
        ImVec4 colOff, ImVec4 colOffHov);
};

// ── TextInput ────────────────────────────────────────────────
// Invisible bg, animated accent underline, cursor blinking
class TextInput : public Widget
{
public:
    // Returns true when Enter is pressed or field loses focus
    // buf/bufLen: the string buffer to edit
    static bool Draw(const char* id, char* buf, int bufLen,
        float width,
        const char* placeholder = "",
        ImVec4 accentColor = ImVec4(1, 1, 1, 1));
};

// ── ValueCell ────────────────────────────────────────────────
// Shows a live value string; double-click enters inline edit mode.
// Uses TextInput internally when editing.
class ValueCell : public Widget
{
public:
    struct State {
        bool  editing = false;
        char  editBuf[32] = "";
        bool  focusNext = false;
    };

    // Returns true when a new value was committed (Enter pressed)
    // committedStr contains what the user typed
    static bool Draw(const char* id, State& state,
        const char* liveStr,   // current value string
        bool   frozen,
        float  width,
        std::string& committedStr);
};

// ── StatusBar ────────────────────────────────────────────────
class StatusBar : public Widget
{
public:
    static void Draw(float y, float w, float h,
        const char* msg, bool isError);
};