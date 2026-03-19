// ============================================================
//  main.cpp  -  CTTrainer Entry Point
//  ImGui + DirectX11 + Win32
//
//  BUILD REQUIREMENTS (Visual Studio):
//  • Add imgui folder to: C/C++ > General > Additional Include Dirs
//  • Linker > Input: d3d11.lib  dxgi.lib  dwmapi.lib
//  • Linker > System > SubSystem: Windows
//  • Run as Administrator
// ============================================================

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "CheatManager.h"
#include "UI.h"
#include "Theme.h"
#include "Logger.h"

#include <d3d11.h>
#include <dwmapi.h>
#include <Windows.h>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxgi.lib")

// ── D3D globals (non-static so UI.cpp can reach them) ────────
ID3D11Device*            g_device = nullptr;
ID3D11DeviceContext*     g_ctx    = nullptr;
static IDXGISwapChain*   g_chain  = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
HWND                     g_hWnd   = nullptr;

// ── D3D helpers ──────────────────────────────────────────────
static void CreateRenderTarget()
{
    ID3D11Texture2D* bb = nullptr;
    g_chain->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (bb) { g_device->CreateRenderTargetView(bb, nullptr, &g_rtv); bb->Release(); }
}
static void CleanupRenderTarget()
{
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
}
static bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL lvls[2] = { D3D_FEATURE_LEVEL_11_0,
                                          D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        lvls, 2, D3D11_SDK_VERSION,
        &sd, &g_chain, &g_device, &fl, &g_ctx);
    if (FAILED(hr)) return false;
    CreateRenderTarget();
    return true;
}
static void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_chain)  { g_chain->Release();  g_chain  = nullptr; }
    if (g_ctx)    { g_ctx->Release();    g_ctx    = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }
}

// ── Window-effects API (called by UI.cpp) ────────────────────

void SetWindowAlpha(int alpha) // 0 = fully transparent, 255 = opaque
{
    SetLayeredWindowAttributes(g_hWnd, 0, (BYTE)alpha, LWA_ALPHA);
}

void SetWindowBlur(bool enable)
{
    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = enable ? TRUE : FALSE;
    DwmEnableBlurBehindWindow(g_hWnd, &bb);
}

void SetWindowAcrylic(bool enable)
{
    // DWMWA_SYSTEMBACKDROP_TYPE = 38 (Win11 SDK)
    // DWMSBT_NONE = 1, DWMSBT_ACRYLIC = 3
    DWORD val = enable ? 3 : 1;
    DwmSetWindowAttribute(g_hWnd, 38, &val, sizeof(val));
}

// ── Font atlas rebuild (called by UI.cpp after modifying Fonts) ─
void RebuildFontAtlas()
{
    ImGui_ImplDX11_InvalidateDeviceObjects();
    ImGui_ImplDX11_CreateDeviceObjects();
}

// ── WndProc ──────────────────────────────────────────────────
extern IMGUI_IMPL_API LRESULT
ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static constexpr int RESIZE_BORDER = 7; // px - how thick the grab zone is
static constexpr int MIN_WIDTH     = 560; // minimum resizable width
static constexpr int MIN_HEIGHT    = 420; // minimum resizable height

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg)
    {
    case WM_NCHITTEST:
    {
        // Let DefWindowProc do the default test first
        LRESULT hit = DefWindowProcW(hWnd, msg, wParam, lParam);
        if (hit != HTCLIENT) return hit; // already a frame/caption area

        // Map cursor to window-relative coords
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT  rc;
        GetWindowRect(hWnd, &rc);

        const bool onLeft   = pt.x < rc.left   + RESIZE_BORDER;
        const bool onRight  = pt.x > rc.right   - RESIZE_BORDER;
        const bool onTop    = pt.y < rc.top     + RESIZE_BORDER;
        const bool onBottom = pt.y > rc.bottom  - RESIZE_BORDER;

        if (onLeft  && onTop)    return HTTOPLEFT;
        if (onRight && onTop)    return HTTOPRIGHT;
        if (onLeft  && onBottom) return HTBOTTOMLEFT;
        if (onRight && onBottom) return HTBOTTOMRIGHT;
        if (onLeft)              return HTLEFT;
        if (onRight)             return HTRIGHT;
        if (onTop)               return HTTOP;
        if (onBottom)            return HTBOTTOM;

        return HTCLIENT;
    }
    case WM_GETMINMAXINFO:
    {
        // Enforce minimum window size so the UI never becomes unusable
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = MIN_WIDTH;
        mmi->ptMinTrackSize.y = MIN_HEIGHT;
        return 0;
    }
    case WM_SIZE:
        if (g_device && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_chain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam),
                DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_MOUSEACTIVATE:
        return MA_ACTIVATE; // pass click through on activation
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ── Font loading helper ───────────────────────────────────────
// Returns the index of the added font, or -1 on failure.
// Fonts are stored in ImGui's atlas; call RebuildFontAtlas() after.
struct FontDef { const char* label; const char* path; float size; };

static const FontDef k_Fonts[] = {
    { "Segoe UI",      "C:\\Windows\\Fonts\\segoeui.ttf",      15.f },
    { "Consolas",      "C:\\Windows\\Fonts\\consola.ttf",       14.f },
    { "Cascadia Code", "C:\\Windows\\Fonts\\CascadiaCode.ttf",  14.f },
    { "Arial",         "C:\\Windows\\Fonts\\arial.ttf",         15.f },
};
static const int k_FontCount = (int)(sizeof(k_Fonts) / sizeof(k_Fonts[0]));

// Loaded ImFont* pointers (null = unavailable on this system)
ImFont* g_Fonts[4] = {};

static void LoadSystemFonts(ImGuiIO& io)
{
    for (int i = 0; i < k_FontCount; ++i)
    {
        if (GetFileAttributesA(k_Fonts[i].path) != INVALID_FILE_ATTRIBUTES)
            g_Fonts[i] = io.Fonts->AddFontFromFileTTF(k_Fonts[i].path, k_Fonts[i].size);
    }
    // Fallback: default ProggyClean if nothing loaded
    bool anyLoaded = false;
    for (int i = 0; i < k_FontCount; ++i) if (g_Fonts[i]) { anyLoaded = true; break; }
    if (!anyLoaded)
        io.Fonts->AddFontDefault();
}

// ── WinMain ──────────────────────────────────────────────────
int WINAPI WinMain (_In_ HINSTANCE hInstance,_In_opt_ HINSTANCE hPrevInstance,_In_ LPSTR lpCmdLine,_In_ int nShowCmd)
{
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0, 0,
                       hInstance, nullptr, nullptr, nullptr, nullptr,
                       L"CTTrainer", nullptr };
    RegisterClassExW(&wc);

    // WS_EX_LAYERED enables SetLayeredWindowAttributes (transparency)
    g_hWnd = CreateWindowExW(WS_EX_LAYERED, wc.lpszClassName, L"CT Trainer  v1.0",
        WS_POPUP,
        100, 100, 760, 620, nullptr, nullptr, hInstance, nullptr);

    // Start fully opaque
    SetLayeredWindowAttributes(g_hWnd, 0, 255, LWA_ALPHA);

    if (!CreateDeviceD3D(g_hWnd))
    {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, hInstance);
        return 1;
    }

    ShowWindow(g_hWnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hWnd);

    // Force sharp corners (Windows 11)
    DWM_WINDOW_CORNER_PREFERENCE noRound = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(g_hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &noRound, sizeof(noRound));

    // ── ImGui setup ───────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    // Load system fonts BEFORE backend init so atlas is built once
    LoadSystemFonts(io);

    // Apply first available font as default
    for (int i = 0; i < k_FontCount; ++i)
    {
        if (g_Fonts[i]) { io.FontDefault = g_Fonts[i]; break; }
    }

    ThemeManager::Get().ApplyCurrent();

    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(g_device, g_ctx);

    LOG_INFO("CTTrainer started  -  v1.0");
    LOG_INFO("Build: " + std::string(__DATE__) + "  " + __TIME__);

    // ── Cheat Manager ─────────────────────────────────────
    CheatManager mgr;

    // ── Message loop ──────────────────────────────────────
    const float BG[4] = { 0.f, 0.f, 0.f, 0.f }; // transparent clear (DWM handles bg)
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        UI::Render(mgr);

        ImGui::Render();
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, BG);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_chain->Present(1, 0);
    }

    mgr.StopAll();
    LOG_INFO("CTTrainer shutting down");
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(g_hWnd);
    UnregisterClassW(wc.lpszClassName, hInstance);
    return 0;
}