// main.cpp -- VL Lab Mechanics Suite: the window, Direct3D 11, Dear ImGui, screen capture for video.
#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <string>
#include "app.hpp"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "implot.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static ID3D11Device* g_dev = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static IDXGISwapChain* g_swap = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static UINT g_resizeW = 0, g_resizeH = 0;
static HWND g_hwnd = nullptr;
static vlab::App* g_app = nullptr;
static bool g_quit = false;

// ====================================================================== the platform services the app uses
struct DxPlatform : vlab::Platform {
    ID3D11Texture2D* staging = nullptr;
    UINT sw = 0, sh = 0;
    bool mapped = false;
    ~DxPlatform() override { if (staging) staging->Release(); }
    bool grabFrame(int x, int y, int w, int h, const void** pixels, int* rowBytes) override {
        ID3D11Texture2D* bb = nullptr;
        if (FAILED(g_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb))) return false;
        D3D11_TEXTURE2D_DESC d;
        bb->GetDesc(&d);
        if (x < 0 || y < 0 || x + w > (int)d.Width || y + h > (int)d.Height) { bb->Release(); return false; }
        if (!staging || sw != d.Width || sh != d.Height) {
            if (staging) { staging->Release(); staging = nullptr; }
            D3D11_TEXTURE2D_DESC sd = d;
            sd.Usage = D3D11_USAGE_STAGING;
            sd.BindFlags = 0;
            sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            sd.MiscFlags = 0;
            sd.SampleDesc.Count = 1;
            sd.SampleDesc.Quality = 0;
            if (FAILED(g_dev->CreateTexture2D(&sd, nullptr, &staging))) { bb->Release(); return false; }
            sw = d.Width;
            sh = d.Height;
        }
        g_ctx->CopyResource(staging, bb);
        bb->Release();
        D3D11_MAPPED_SUBRESOURCE ms;
        if (FAILED(g_ctx->Map(staging, 0, D3D11_MAP_READ, 0, &ms))) return false;
        mapped = true;
        *pixels = (const unsigned char*)ms.pData + (size_t)y * ms.RowPitch + (size_t)x * 4;
        *rowBytes = (int)ms.RowPitch;
        return true;
    }
    void releaseFrame() override { if (mapped && staging) { g_ctx->Unmap(staging, 0); mapped = false; } }
    void setTitle(const std::string& t) override { SetWindowTextA(g_hwnd, t.c_str()); }
    void requestQuit() override { PostMessage(g_hwnd, WM_CLOSE, 0, 0); }
    struct Tex { ID3D11Texture2D* tex = nullptr; ID3D11ShaderResourceView* srv = nullptr; int w = 0, h = 0; };
    void* createTexture(int w, int h) override {
        D3D11_TEXTURE2D_DESC d{};
        d.Width = (UINT)w; d.Height = (UINT)h; d.MipLevels = 1; d.ArraySize = 1;
        d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DYNAMIC;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        Tex* t = new Tex();
        t->w = w; t->h = h;
        if (FAILED(g_dev->CreateTexture2D(&d, nullptr, &t->tex)) || FAILED(g_dev->CreateShaderResourceView(t->tex, nullptr, &t->srv))) { destroyTexture(t); return nullptr; }
        return t;
    }
    void updateTexture(void* h, const void* bgra, int pitchBytes) override {
        Tex* t = (Tex*)h;
        if (!t || !t->tex) return;
        D3D11_MAPPED_SUBRESOURCE ms;
        if (FAILED(g_ctx->Map(t->tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return;
        for (int y = 0; y < t->h; ++y) memcpy((unsigned char*)ms.pData + (size_t)y * ms.RowPitch, (const unsigned char*)bgra + (size_t)y * pitchBytes, (size_t)t->w * 4);
        g_ctx->Unmap(t->tex, 0);
    }
    void destroyTexture(void* h) override {
        Tex* t = (Tex*)h;
        if (!t) return;
        if (t->srv) t->srv->Release();
        if (t->tex) t->tex->Release();
        delete t;
    }
    unsigned long long textureId(void* h) override { Tex* t = (Tex*)h; return t ? (unsigned long long)(uintptr_t)t->srv : 0; }
};

// ====================================================================== Direct3D
static void createRenderTarget() {
    ID3D11Texture2D* bb = nullptr;
    g_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
    g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv);
    bb->Release();
}
static void cleanupRenderTarget() { if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; } }

static bool createDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;          // BGRA: the layout the video encoder expects
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL levels[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &g_swap, &g_dev, &got, &g_ctx);
    if (hr == DXGI_ERROR_UNSUPPORTED)
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &g_swap, &g_dev, &got, &g_ctx);
    if (FAILED(hr)) return false;
    createRenderTarget();
    return true;
}
static void cleanupDevice() {
    cleanupRenderTarget();
    if (g_swap) { g_swap->Release(); g_swap = nullptr; }
    if (g_ctx) { g_ctx->Release(); g_ctx = nullptr; }
    if (g_dev) { g_dev->Release(); g_dev = nullptr; }
}

// ====================================================================== window procedure
static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;
    switch (msg) {
        case WM_SIZE:
            if (wp == SIZE_MINIMIZED) return 0;
            g_resizeW = LOWORD(lp);
            g_resizeH = HIWORD(lp);
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mi = (MINMAXINFO*)lp;
            mi->ptMinTrackSize.x = 900;
            mi->ptMinTrackSize.y = 600;
            return 0;
        }
        case WM_SYSCOMMAND:
            if ((wp & 0xfff0) == SC_KEYMENU) return 0;            // do not let a stray Alt key steal the keyboard
            break;
        case WM_CLOSE:
            if (g_app && g_app->dirty) {
                int r = MessageBoxA(hwnd, "Save the changes to this design before closing?", "VL Lab Mechanics Suite", MB_YESNOCANCEL | MB_ICONQUESTION);
                if (r == IDCANCEL) return 0;
                if (r == IDYES && !g_app->saveDesign(false)) return 0;
            }
            g_quit = true;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void applyStyle(float dpi) {
    ImGuiStyle& s = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    s.WindowRounding = 4;
    s.FrameRounding = 4;
    s.GrabRounding = 4;
    s.TabRounding = 4;
    s.ScrollbarRounding = 6;
    s.PopupRounding = 4;
    s.FramePadding = ImVec2(8, 4);
    s.ItemSpacing = ImVec2(8, 6);
    s.WindowBorderSize = 1;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.086f, 0.094f, 0.118f, 1);
    c[ImGuiCol_ChildBg] = ImVec4(0.075f, 0.082f, 0.104f, 1);
    c[ImGuiCol_PopupBg] = ImVec4(0.11f, 0.12f, 0.15f, 0.98f);
    c[ImGuiCol_Border] = ImVec4(0.20f, 0.22f, 0.28f, 1);
    c[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.145f, 0.18f, 1);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.17f, 0.19f, 0.24f, 1);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.23f, 0.29f, 1);
    c[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.075f, 0.095f, 1);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.115f, 0.15f, 1);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.07f, 0.075f, 0.095f, 1);
    c[ImGuiCol_Button] = ImVec4(0.17f, 0.20f, 0.26f, 1);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.23f, 0.28f, 0.37f, 1);
    c[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.45f, 0.95f, 1);
    c[ImGuiCol_Header] = ImVec4(0.17f, 0.30f, 0.55f, 0.55f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.38f, 0.70f, 0.70f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.45f, 0.95f, 0.80f);
    c[ImGuiCol_Tab] = ImVec4(0.11f, 0.125f, 0.16f, 1);
    c[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.45f, 0.95f, 0.80f);
    c[ImGuiCol_TabSelected] = ImVec4(0.17f, 0.36f, 0.75f, 1);
    c[ImGuiCol_TabDimmed] = ImVec4(0.09f, 0.10f, 0.13f, 1);
    c[ImGuiCol_TabDimmedSelected] = ImVec4(0.14f, 0.28f, 0.58f, 1);
    c[ImGuiCol_CheckMark] = ImVec4(0.45f, 0.78f, 1.0f, 1);
    c[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.58f, 1.0f, 1);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.70f, 1.0f, 1);
    c[ImGuiCol_DockingPreview] = ImVec4(0.20f, 0.45f, 0.95f, 0.5f);
    s.ScaleAllSizes(dpi);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    SetProcessDPIAware();
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof wc;
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconA(hInst, MAKEINTRESOURCEA(1));
    wc.hIconSm = LoadIconA(hInst, MAKEINTRESOURCEA(1));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "VLabSuiteWindow";
    RegisterClassExA(&wc);
    g_hwnd = CreateWindowExA(0, wc.lpszClassName, "VL Lab Mechanics Suite", WS_OVERLAPPEDWINDOW, 80, 60, 1600, 940, nullptr, nullptr, hInst, nullptr);
    if (!createDevice(g_hwnd)) {
        MessageBoxA(nullptr, "Could not start Direct3D 11. Please update your graphics driver.", "VL Lab Mechanics Suite", MB_ICONERROR);
        cleanupDevice();
        return 1;
    }
    ShowWindow(g_hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);
    float dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(g_hwnd);
    if (dpi < 1.0f) dpi = 1.0f;
    applyStyle(dpi);
    {   // a clean UI font: Segoe UI ships with every Windows
        char winDir[MAX_PATH];
        GetWindowsDirectoryA(winDir, MAX_PATH);
        std::string font = std::string(winDir) + "\\Fonts\\segoeui.ttf";
        if (GetFileAttributesA(font.c_str()) != INVALID_FILE_ATTRIBUTES) io.Fonts->AddFontFromFileTTF(font.c_str(), 17.0f * dpi);
        else io.Fonts->AddFontDefault();
    }

    DxPlatform platform;
    vlab::App app;
    g_app = &app;
    app.init(&platform);
    {   // "--autotest FOLDER": drive the real UI through runs, export and screenshots, then quit (used for testing)
        std::string cmd = GetCommandLineA();
        size_t k = cmd.find("--autotest ");
        if (k != std::string::npos) {
            std::string dir = cmd.substr(k + 11);
            while (!dir.empty() && (dir.front() == ' ' || dir.front() == '"')) dir.erase(dir.begin());
            while (!dir.empty() && (dir.back() == ' ' || dir.back() == '"')) dir.pop_back();
            app.autotestDir = dir;
            app.recordingsDir = dir;
        }
    }
    static std::string iniPath = app.docsDir + "\\layout.ini";
    io.IniFilename = iniPath.c_str();
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);

    const float clear[4] = {0.07f, 0.075f, 0.095f, 1.0f};
    MSG msg{};
    while (!g_quit) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_quit = true;
        }
        if (g_quit) break;
        if (IsIconic(g_hwnd)) { Sleep(20); continue; }
        if (g_resizeW != 0 && g_resizeH != 0) {
            cleanupRenderTarget();
            g_swap->ResizeBuffers(0, g_resizeW, g_resizeH, DXGI_FORMAT_UNKNOWN, 0);
            g_resizeW = g_resizeH = 0;
            createRenderTarget();
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        app.frame(std::min(io.DeltaTime, 0.1f));
        ImGui::Render();
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        app.endOfFrameCapture();
        if (app.autotestQuit) g_quit = true;                                  // the picture is complete: write it to the video if one is due
        g_swap->Present(1, 0);
    }
    app.shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    cleanupDevice();
    DestroyWindow(g_hwnd);
    UnregisterClassA(wc.lpszClassName, hInst);
    return 0;
}
