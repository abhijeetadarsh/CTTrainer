// ============================================================
//  Widgets.cpp  --  Custom widget implementations
// ============================================================
#define IMGUI_DEFINE_MATH_OPERATORS   // enables ImVec2 +, -, *, / operators
#include "../include/Widgets.h"
#include <unordered_map>
#include <cstring>
#include <cmath>

// ── Animation state store ────────────────────────────────────
namespace Anim
{
    static std::unordered_map<ImGuiID, float> s_vals;

    float Towards(ImGuiID id, float target, float speed)
    {
        float dt = ImGui::GetIO().DeltaTime;
        auto  it = s_vals.find(id);
        float cur = (it != s_vals.end()) ? it->second : target;
        float dir = target - cur;
        float step = speed * dt;
        if (fabsf(dir) <= step) cur = target;
        else                    cur += (dir > 0 ? step : -step);
        s_vals[id] = cur;
        return cur;
    }
}

// ── Widget base ──────────────────────────────────────────────

ImRect Widget::Reserve(float w, float h)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(w, h));
    return ImRect(pos, ImVec2(pos.x + w, pos.y + h));
}

bool Widget::IsHovered(const ImRect& r)
{
    ImVec2 mp = ImGui::GetIO().MousePos;
    return r.Contains(mp) && ImGui::IsWindowHovered(
        ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
}

bool Widget::IsClicked(const ImRect& r, int btn)
{
    return IsHovered(r) && ImGui::IsMouseClicked(btn);
}

bool Widget::IsDblClicked(const ImRect& r)
{
    return IsHovered(r) && ImGui::IsMouseDoubleClicked(0);
}

void Widget::DrawRoundRect(ImDrawList* dl, ImRect r, ImU32 col,
    float rounding, float thickness)
{
    if (thickness <= 0.f)
        dl->AddRectFilled(r.Min, r.Max, col, rounding);
    else
        dl->AddRect(r.Min, r.Max, col, rounding, 0, thickness);
}

void Widget::DrawGlow(ImDrawList* dl, ImRect r, ImU32 col,
    float rounding, float spread)
{
    // Layered transparent rects expanding outward = soft glow
    ImU32 baseA = (col & 0x00FFFFFF) | 0x28000000;
    ImU32 midA = (col & 0x00FFFFFF) | 0x18000000;
    ImU32 farA = (col & 0x00FFFFFF) | 0x08000000;
    ImRect r1 = ImRect(r.Min - ImVec2(spread * 0.5f, spread * 0.5f),
        r.Max + ImVec2(spread * 0.5f, spread * 0.5f));
    ImRect r2 = ImRect(r.Min - ImVec2(spread, spread),
        r.Max + ImVec2(spread, spread));
    ImRect r3 = ImRect(r.Min - ImVec2(spread * 1.8f, spread * 1.8f),
        r.Max + ImVec2(spread * 1.8f, spread * 1.8f));
    dl->AddRectFilled(r3.Min, r3.Max, farA, rounding + spread * 1.8f);
    dl->AddRectFilled(r2.Min, r2.Max, midA, rounding + spread);
    dl->AddRectFilled(r1.Min, r1.Max, baseA, rounding + spread * 0.5f);
}

void Widget::DrawCenteredText(ImDrawList* dl, ImRect r,
    ImU32 col, const char* text)
{
    // Build a clipped version of the label that fits inside the rect
    const float maxW = r.GetWidth() - 10.f;   // 5px padding each side
    ImVec2      ts = ImGui::CalcTextSize(text);
    const char* drawTxt = text;
    char        clipped[128];

    if (ts.x > maxW && maxW > 0.f)
    {
        // Binary-search the longest prefix that fits (with "…" appended)
        strncpy_s(clipped, sizeof(clipped), text, _TRUNCATE);
        int len = (int)strlen(clipped);
        while (len > 0)
        {
            clipped[len] = '\0';
            // append ellipsis indicator
            char tmp[132];
            snprintf(tmp, sizeof(tmp), "%s..", clipped);
            if (ImGui::CalcTextSize(tmp).x <= maxW || len == 1)
            {
                strncpy_s(clipped, sizeof(clipped), tmp, _TRUNCATE); break;
            }
            --len;
        }
        drawTxt = clipped;
        ts = ImGui::CalcTextSize(drawTxt);
    }

    ImVec2 pos = ImVec2(r.Min.x + (r.GetWidth() - ts.x) * 0.5f,
        r.Min.y + (r.GetHeight() - ts.y) * 0.5f);
    dl->AddText(pos, col, drawTxt);
}

// ── GlowButton ───────────────────────────────────────────────

bool GlowButton::Draw(const char* id, const char* label,
    ImVec2 size,
    ImVec4 colFill, ImVec4 colFillHov,
    ImVec4 colGlow,
    float rounding)
{
    ImGuiID  wid = ImGui::GetID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImRect r = Reserve(size.x, size.y);
    bool   hov = IsHovered(r);
    bool   clicked = IsClicked(r);
    bool   held = hov && ImGui::IsMouseDown(0);

    // Animate hover 0..1
    float t = Anim::Towards(wid, hov ? 1.f : 0.f, 6.f);
    float tp = Anim::Towards(wid + 1, held ? 1.f : 0.f, 12.f);

    // Press: shrink slightly
    float shrink = tp * 2.f;
    ImRect rd = ImRect(r.Min + ImVec2(shrink, shrink),
        r.Max - ImVec2(shrink, shrink));

    // Background fill lerped between idle/hover
    ImVec4 fill = Anim::LerpColor(colFill, colFillHov, Anim::Smooth(t));
    DrawRoundRect(dl, rd, Anim::ColorU32(fill), rounding);

    // Glow on hover
    if (t > 0.01f)
        DrawGlow(dl, rd, Anim::ColorU32(colGlow), rounding, t * 6.f);

    // Accent border (always subtle, brighter on hover)
    ImVec4 border = Anim::LerpColor(
        ImVec4(colGlow.x, colGlow.y, colGlow.z, 0.25f),
        ImVec4(colGlow.x, colGlow.y, colGlow.z, 0.80f), t);
    DrawRoundRect(dl, rd, Anim::ColorU32(border), rounding, 1.f);

    // Label
    ImU32 textCol = Anim::ColorU32(
        Anim::LerpColor(ImVec4(0.8f, 0.8f, 0.9f, 1), ImVec4(1, 1, 1, 1), t));
    DrawCenteredText(dl, rd, textCol, label);

    return clicked;
}

// ── GhostButton ──────────────────────────────────────────────

bool GhostButton::Draw(const char* id, const char* label,
    ImVec2 size,
    ImVec4 colBorder, ImVec4 colHovFill,
    float rounding)
{
    ImGuiID     wid = ImGui::GetID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImRect      r = Reserve(size.x, size.y);
    bool        hov = IsHovered(r);
    bool      click = IsClicked(r);

    float t = Anim::Towards(wid, hov ? 1.f : 0.f, 7.f);

    // Hover fill
    if (t > 0.01f)
    {
        ImVec4 fc = ImVec4(colHovFill.x, colHovFill.y,
            colHovFill.z, colHovFill.w * Anim::Smooth(t));
        DrawRoundRect(dl, r, Anim::ColorU32(fc), rounding);
    }

    // Border: lerp opacity + glow
    ImVec4 bc = Anim::LerpColor(
        ImVec4(colBorder.x, colBorder.y, colBorder.z, 0.45f),
        ImVec4(colBorder.x, colBorder.y, colBorder.z, 1.00f), t);
    DrawRoundRect(dl, r, Anim::ColorU32(bc), rounding, 1.4f);

    if (t > 0.05f)
        DrawGlow(dl, r, Anim::ColorU32(colBorder), rounding, t * 4.f);

    ImU32 tc = Anim::ColorU32(
        Anim::LerpColor(
            ImVec4(colBorder.x, colBorder.y, colBorder.z, 0.75f),
            ImVec4(1.f, 1.f, 1.f, 1.f), t));
    DrawCenteredText(dl, r, tc, label);

    return click;
}

// ── PillButton ────────────────────────────────────────────────

bool PillButton::Draw(const char* id, const char* labelOn,
    const char* labelOff, bool active,
    ImVec2 size,
    ImVec4 colOn, ImVec4 colOnHov,
    ImVec4 colOff, ImVec4 colOffHov)
{
    ImGuiID     wid = ImGui::GetID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float pill = size.y * 0.5f;   // fully round ends
    ImRect r = Reserve(size.x, size.y);
    bool   hov = IsHovered(r);
    bool   click = IsClicked(r);

    float th = Anim::Towards(wid, hov ? 1.f : 0.f, 7.f);
    float ton = Anim::Towards(wid + 1, active ? 1.f : 0.f, 9.f);

    ImVec4 idle = Anim::LerpColor(colOff, colOn, ton);
    ImVec4 hvc = Anim::LerpColor(colOffHov, colOnHov, ton);
    ImVec4 fill = Anim::LerpColor(idle, hvc, Anim::Smooth(th));

    DrawRoundRect(dl, r, Anim::ColorU32(fill), pill);

    // Inner highlight strip (top sheen)
    ImVec2 sheenMin = ImVec2(r.Min.x + 4.f, r.Min.y + 2.f);
    ImVec2 sheenMax = ImVec2(r.Max.x - 4.f, r.Min.y + size.y * 0.45f);
    ImRect sheen = ImRect(sheenMin, sheenMax);
    ImU32 sheenCol = IM_COL32(255, 255, 255, (int)(25 + 20 * ton));
    dl->AddRectFilled(sheen.Min, sheen.Max,
        sheenCol, pill, ImDrawFlags_RoundCornersTop);

    const char* lbl = active ? labelOn : labelOff;
    DrawCenteredText(dl, r, IM_COL32(255, 255, 255, 230), lbl);

    return click;
}

// ── TextInput ────────────────────────────────────────────────

bool TextInput::Draw(const char* id, char* buf, int bufLen,
    float width, const char* placeholder,
    ImVec4 accentColor)
{
    const float H = 28.f;
    ImGuiID     wid = ImGui::GetID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImRect      r = Reserve(width, H);

    // Use an invisible ImGui InputText for actual keyboard handling
    // Position it exactly over our rect
    ImGui::SetCursorScreenPos(r.Min);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, accentColor);
    ImGui::SetNextItemWidth(width);

    bool committed = ImGui::InputText(id, buf, bufLen,
        ImGuiInputTextFlags_EnterReturnsTrue);
    bool focused = ImGui::IsItemFocused();
    bool hov = IsHovered(r);
    ImGui::PopStyleColor(5);

    // Animated focus level
    float tf = Anim::Towards(wid, focused ? 1.f : 0.f, 8.f);

    // Bottom underline: thin grey, expands to accent on focus
    float lineH = 1.5f + tf * 0.5f;
    ImVec4 lineCol = Anim::LerpColor(
        ImVec4(0.3f, 0.3f, 0.4f, 0.5f), accentColor, Anim::Smooth(tf));
    float underY = r.Max.y - 1.f;
    // Full underline (dim)
    dl->AddLine(ImVec2(r.Min.x, underY), ImVec2(r.Max.x, underY),
        IM_COL32(60, 60, 80, 120), 1.f);
    // Animated fill from center
    float hw = (r.GetWidth() * 0.5f) * Anim::Smooth(tf);
    float cx = r.Min.x + r.GetWidth() * 0.5f;
    if (hw > 0.5f)
        dl->AddLine(ImVec2(cx - hw, underY), ImVec2(cx + hw, underY),
            Anim::ColorU32(lineCol), lineH);

    // Placeholder text when empty and unfocused
    if (!focused && buf[0] == '\0' && placeholder[0] != '\0')
    {
        ImVec4 phCol = ImVec4(accentColor.x, accentColor.y,
            accentColor.z, 0.30f);
        dl->AddText(ImVec2(r.Min.x + 2.f, r.Min.y + 5.f),
            Anim::ColorU32(phCol), placeholder);
    }

    return committed;
}

// ── ValueCell ────────────────────────────────────────────────

bool ValueCell::Draw(const char* id, State& st,
    const char* liveStr, bool frozen,
    float width, std::string& committedStr)
{
    ImGuiID     wid = ImGui::GetID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float H = 24.f;
    const ThemePalette& p = ThemeManager::Get().Palette();
    bool committed = false;

    if (st.editing)
    {
        // ── Edit mode: custom TextInput ───────────────────────
        ImVec4 accent = p.accent;
        if (st.focusNext)
        {
            ImGui::SetNextItemWidth(width);
            // Focus next InputText – set via ImGui internals
            ImGui::SetKeyboardFocusHere(0);
            st.focusNext = false;
        }
        bool ok = TextInput::Draw(id, st.editBuf, sizeof(st.editBuf),
            width, "", accent);
        if (ok)
        {
            committedStr = st.editBuf;
            committed = true;
            st.editing = false;
        }
        // Click away cancels
        bool focused = ImGui::IsItemFocused();
        if (!focused && !st.focusNext)
            st.editing = false;
    }
    else
    {
        // ── Display mode ──────────────────────────────────────
        ImRect r = Reserve(width, H);
        bool   hov = IsHovered(r);
        bool   dbl = IsDblClicked(r);

        float th = Anim::Towards(wid, hov ? 1.f : 0.f, 7.f);

        // Subtle hover bg
        if (th > 0.01f)
        {
            ImVec4 hb = ImVec4(p.accent.x, p.accent.y,
                p.accent.z, 0.08f * Anim::Smooth(th));
            DrawRoundRect(dl, r, Anim::ColorU32(hb), 3.f);
        }

        // Value text color
        ImVec4 textCol;
        if (strcmp(liveStr, "--") == 0)
            textCol = p.textDisabled;
        else if (frozen)
            textCol = p.frozen;
        else
            textCol = Anim::LerpColor(p.textSecondary, p.textPrimary,
                Anim::Smooth(th));

        // Frozen: small indicator dot on left
        float textOffX = 4.f;
        if (frozen)
        {
            float pulse = (sinf((float)ImGui::GetTime() * 3.f) + 1.f) * 0.5f;
            ImU32 dotCol = Anim::ColorU32(
                ImVec4(p.frozen.x, p.frozen.y, p.frozen.z, 0.5f + 0.5f * pulse));
            ImVec2 dc = ImVec2(r.Min.x + 6.f, r.Min.y + H * 0.5f);
            dl->AddCircleFilled(dc, 3.f, dotCol);
            textOffX = 14.f;
        }

        // Draw value text left-aligned
        ImVec2 textPos = ImVec2(r.Min.x + textOffX,
            r.Min.y + (H - ImGui::GetTextLineHeight()) * 0.5f);
        dl->AddText(textPos, Anim::ColorU32(textCol), liveStr);

        // Pencil hint on hover
        if (th > 0.2f)
        {
            ImVec4 hint = ImVec4(p.accent.x, p.accent.y,
                p.accent.z, 0.35f * Anim::Smooth(th));
            const char* pencil = "edit";
            ImVec2 ps = ImGui::CalcTextSize(pencil);
            dl->AddText(ImVec2(r.Max.x - ps.x - 4.f,
                r.Min.y + (H - ps.y) * 0.5f),
                Anim::ColorU32(hint), pencil);
        }

        if (dbl)
        {
            st.editing = true;
            st.focusNext = true;
            strncpy_s(st.editBuf, sizeof(st.editBuf), liveStr, _TRUNCATE);
            if (strcmp(st.editBuf, "--") == 0) st.editBuf[0] = '\0';
        }
    }

    return committed;
}

// ── StatusBar ────────────────────────────────────────────────

void StatusBar::Draw(float y, float w, float h,
    const char* msg, bool isError)
{
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ThemePalette& p = ThemeManager::Get().Palette();

    // Background
    dl->AddRectFilled(ImVec2(0, y), ImVec2(w, y + h),
        Anim::ColorU32(p.bgDeep));
    // Top border line
    dl->AddLine(ImVec2(0, y), ImVec2(w, y),
        Anim::ColorU32(p.border), 1.f);

    // Animated status dot
    float pulse = (sinf((float)ImGui::GetTime() * (isError ? 4.f : 1.8f)) + 1.f) * 0.5f;
    ImVec4 dotBase = isError ? p.bad : p.good;
    ImU32  dotCol = Anim::ColorU32(
        ImVec4(dotBase.x, dotBase.y, dotBase.z, 0.5f + 0.5f * pulse));
    dl->AddCircleFilled(ImVec2(12.f, y + h * 0.5f), 4.f, dotCol);
    // dot outer ring
    dl->AddCircle(ImVec2(12.f, y + h * 0.5f), 5.f,
        Anim::ColorU32(ImVec4(dotBase.x, dotBase.y, dotBase.z, 0.2f)), 0, 1.f);

    // Message text
    ImVec4 textCol = isError ? p.bad : p.textSecondary;
    dl->AddText(ImVec2(24.f, y + (h - ImGui::GetTextLineHeight()) * 0.5f),
        Anim::ColorU32(textCol), msg);
}