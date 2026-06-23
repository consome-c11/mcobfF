#include <windows.h>
#include <dwmapi.h>
#include <d3d11.h>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include "gui/AppState.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (FAILED(hr)) return false;

    ID3D11Texture2D* backBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) return false;
    g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_mainRenderTargetView);
    backBuffer->Release();
    return true;
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_SIZE:
            if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                ID3D11Texture2D* backBuffer = nullptr;
                g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
                g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_mainRenderTargetView);
                backBuffer->Release();
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_NCHITTEST: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT rect;
            GetWindowRect(hWnd, &rect);
            const int border = 8;
            int x = pt.x - rect.left;
            int y = pt.y - rect.top;
            int w = rect.right - rect.left;
            int h = rect.bottom - rect.top;
            if (x < border && y < border) return HTTOPLEFT;
            if (x >= w - border && y < border) return HTTOPRIGHT;
            if (x < border && y >= h - border) return HTBOTTOMLEFT;
            if (x >= w - border && y >= h - border) return HTBOTTOMRIGHT;
            if (x < border) return HTLEFT;
            if (x >= w - border) return HTRIGHT;
            if (y < border) return HTTOP;
            if (y >= h - border) return HTBOTTOM;

            const float titleBarHeight = 36.0f;
            const float btnW = 46.0f;
            float dragEnd = (float)w - btnW * 3;
            if (y < titleBarHeight && y >= border && x < dragEnd) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        case WM_GETMINMAXINFO: {
            RECT work;
            if (SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0)) {
                ((MINMAXINFO*)lParam)->ptMaxPosition.x = work.left;
                ((MINMAXINFO*)lParam)->ptMaxPosition.y = work.top;
                ((MINMAXINFO*)lParam)->ptMaxSize.x = work.right - work.left;
                ((MINMAXINFO*)lParam)->ptMaxSize.y = work.bottom - work.top;
            }
            return 0;
        }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
#ifdef _DEBUG
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
#endif
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
                      hInstance, nullptr, nullptr, nullptr, nullptr,
                      "mcobfF_GUI", nullptr };
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(WS_EX_APPWINDOW, wc.lpszClassName, "MC-OBF-Find",
                               WS_POPUP,
                               100, 100, 1280, 800,
                               nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    {
        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    }
    {
        DWORD cornerPreference = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.WindowPadding     = ImVec2(10.0f, 10.0f);
    style.FramePadding      = ImVec2(6.0f, 4.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
    style.ScrollbarSize     = 14.0f;
    style.GrabMinSize       = 10.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.10f, 0.11f, 0.13f, 0.95f);
    colors[ImGuiCol_Border]                = ImVec4(0.25f, 0.26f, 0.30f, 0.60f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.17f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.22f, 0.45f, 0.78f, 0.40f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.22f, 0.45f, 0.78f, 0.60f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.08f, 0.09f, 0.11f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.11f, 0.12f, 0.14f, 0.50f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.32f, 0.33f, 0.38f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.42f, 0.43f, 0.48f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.52f, 0.53f, 0.58f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.30f, 0.65f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.30f, 0.65f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.40f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.22f, 0.45f, 0.78f, 0.60f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.22f, 0.45f, 0.78f, 0.80f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.22f, 0.45f, 0.78f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.22f, 0.45f, 0.78f, 0.45f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.22f, 0.45f, 0.78f, 0.65f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.22f, 0.45f, 0.78f, 0.80f);
    colors[ImGuiCol_Separator]             = ImVec4(0.25f, 0.26f, 0.30f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.30f, 0.65f, 1.00f, 0.75f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.30f, 0.65f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.30f, 0.65f, 1.00f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.30f, 0.65f, 1.00f, 0.65f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.30f, 0.65f, 1.00f, 0.90f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.15f, 0.32f, 0.58f, 0.80f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.22f, 0.45f, 0.78f, 0.80f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.25f, 0.26f, 0.30f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.20f, 0.21f, 0.25f, 0.60f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    io.Fonts->AddFontDefault();

    AppState appState;
    appState.setHwnd(hwnd);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        appState.renderGui();

        ImGui::Render();
        const FLOAT clearColor[4] = { 0.11f, 0.12f, 0.14f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}
