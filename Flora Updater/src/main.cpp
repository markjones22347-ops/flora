#include "updater.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <windows.h>
#include <shlobj.h>
#include <vector>
#include <string>

// Global state
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static UINT g_ResizeWidth = 0;
static UINT g_ResizeHeight = 0;
static bool g_done = false;
static std::string g_customPath = "";
static std::string g_logOutput = "";
static bool g_isInstalling = false;
static float g_progress = 0.0f;

// Forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

std::string BrowseForFolder() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    
    IFileOpenDialog* pFileOpen = NULL;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, (LPVOID*)&pFileOpen);
    if (FAILED(hr)) {
        if (SUCCEEDED(hr)) CoUninitialize();
        return "";
    }
    
    // Set options for folder picker
    DWORD dwFlags;
    pFileOpen->GetOptions(&dwFlags);
    pFileOpen->SetOptions(dwFlags | FOS_PICKFOLDERS);
    
    // Show dialog
    hr = pFileOpen->Show(NULL);
    if (FAILED(hr)) {
        pFileOpen->Release();
        if (SUCCEEDED(hr)) CoUninitialize();
        return "";
    }
    
    // Get selected folder
    IShellItem* pItem = NULL;
    hr = pFileOpen->GetResult(&pItem);
    if (FAILED(hr)) {
        pFileOpen->Release();
        if (SUCCEEDED(hr)) CoUninitialize();
        return "";
    }
    
    PWSTR pszPath = NULL;
    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
    if (FAILED(hr)) {
        pItem->Release();
        pFileOpen->Release();
        if (SUCCEEDED(hr)) CoUninitialize();
        return "";
    }
    
    // Convert to std::string
    std::string path;
    int size = WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, NULL, 0, NULL, NULL);
    if (size > 0) {
        path.resize(size);
        WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, &path[0], size, NULL, NULL);
        path.resize(size - 1); // Remove null terminator
    }
    
    CoTaskMemFree(pszPath);
    pItem->Release();
    pFileOpen->Release();
    if (SUCCEEDED(hr)) CoUninitialize();
    
    return path;
}

class LoggingUpdater : public Updater {
public:
    LoggingUpdater(const std::string& customPath = "") : Updater(customPath) {}
    
protected:
    void Log(const std::string& message) override {
        g_logOutput += message + "\n";
    }
};

// Main function
int main(int, char**) {
    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"FloraUpdater", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Flora Updater", WS_OVERLAPPEDWINDOW, 100, 100, 600, 500, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Main loop
    while (!g_done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                g_done = true;
        }
        if (g_done)
            break;

        // Handle window resize
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Main window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Flora Updater", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        // Header
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("Flora Updater");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Installation path section
        ImGui::Text("Installation Directory");
        ImGui::Spacing();
        
        char pathBuffer[MAX_PATH] = {};
        strncpy_s(pathBuffer, g_customPath.c_str(), MAX_PATH - 1);
        ImGui::SetNextItemWidth(-60);
        if (ImGui::InputText("##Path", pathBuffer, MAX_PATH)) {
            g_customPath = pathBuffer;
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse", ImVec2(50, 0))) {
            g_customPath = BrowseForFolder();
        }
        
        ImGui::Spacing();
        if (ImGui::Button("Use Default Location")) {
            char appDataPath[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
                g_customPath = std::string(appDataPath) + "\\Flora";
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Install button
        if (!g_isInstalling) {
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 200) * 0.5f);
            if (ImGui::Button("Install / Update", ImVec2(200, 40))) {
                g_isInstalling = true;
                g_logOutput = "";
                g_progress = 0.0f;
                
                // Run updater
                LoggingUpdater updater(g_customPath);
                updater.Run();
                g_isInstalling = false;
                g_progress = 100.0f;
            }
        } else {
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 200) * 0.5f);
            ImGui::Button("Installing...", ImVec2(200, 40));
            ImGui::Spacing();
            ImGui::ProgressBar(g_progress / 100.0f, ImVec2(200, 10));
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Log output
        ImGui::Text("Activity Log");
        ImGui::Spacing();
        ImGui::BeginChild("Log", ImVec2(0, 150), true);
        ImGui::TextUnformatted(g_logOutput.c_str());
        ImGui::EndChild();

        ImGui::End();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.10f, 0.10f, 0.10f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            g_ResizeWidth = (UINT)LOWORD(lParam);
            g_ResizeHeight = (UINT)HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
