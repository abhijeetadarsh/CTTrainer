#pragma once
// ============================================================
//  UI.h  –  All ImGui rendering lives here
// ============================================================
#include "CheatManager.h"
#include <string>

namespace UI
{
    void Render(CheatManager& mgr);

    // Shared status bar
    void SetStatus(const std::string& msg, bool error = false);
}