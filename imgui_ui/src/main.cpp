#include <windows.h>
#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <thread>
#include <ShlObj.h>

using json = nlohmann::json;

// Global variables
HWND g_hWnd = NULL;
ID3D11Device* g_pd3dDevice = NULL;
ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
IDXGISwapChain* g_pSwapChain = NULL;
ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

// Settings
struct Settings {
    bool alwaysOnTop = false;
    bool autoAttach = false;
    int fontSize = 14;
} g_Settings;

// FloraAPI
HMODULE g_FloraAPI = NULL;

// Function pointers from FloraAPI (matches exports.h)
typedef bool (*InitializeFunc)();
typedef DWORD (*FindRobloxProcessFunc)();
typedef bool (*ConnectFunc)(DWORD pid);
typedef void (*DisconnectFunc)();
typedef DWORD (*GetRobloxPidFunc)();
typedef int (*ExecuteScriptFunc)(const char* source, int sourceLen);
typedef int (*GetLastExecErrorFunc)(char* buffer, int bufLen);

ConnectFunc g_Connect = NULL;
DisconnectFunc g_Disconnect = NULL;
ExecuteScriptFunc g_ExecuteScript = NULL;
GetRobloxPidFunc g_GetRobloxPid = NULL;
InitializeFunc g_Initialize = NULL;
FindRobloxProcessFunc g_FindRobloxProcess = NULL;

// Logs
std::vector<std::string> g_logs;

void AddLog(const std::string& log) {
    g_logs.push_back(log);
    if (g_logs.size() > 100) g_logs.erase(g_logs.begin());
}

void LoadFloraAPI() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string exePath(path);
    size_t lastSlash = exePath.find_last_of("\\");
    if (lastSlash != std::string::npos) {
        exePath = exePath.substr(0, lastSlash);
    }
    std::string dllPath = exePath + "\\FloraAPI.dll";
    
    AddLog("[INIT] Loading FloraAPI from: " + dllPath);
    
    g_FloraAPI = LoadLibraryA(dllPath.c_str());
    if (!g_FloraAPI) {
        DWORD error = GetLastError();
        AddLog("[ERROR] Failed to load FloraAPI.dll. Error: " + std::to_string(error));
        return;
    }
    
    AddLog("[INIT] FloraAPI.dll loaded successfully");
    
    // Load actual exported functions from exports.h
    g_Initialize = (InitializeFunc)GetProcAddress(g_FloraAPI, "Initialize");
    g_FindRobloxProcess = (FindRobloxProcessFunc)GetProcAddress(g_FloraAPI, "FindRobloxProcess");
    g_Connect = (ConnectFunc)GetProcAddress(g_FloraAPI, "Connect");
    g_Disconnect = (DisconnectFunc)GetProcAddress(g_FloraAPI, "Disconnect");
    g_GetRobloxPid = (GetRobloxPidFunc)GetProcAddress(g_FloraAPI, "GetRobloxPid");
    g_ExecuteScript = (ExecuteScriptFunc)GetProcAddress(g_FloraAPI, "ExecuteScript");
    
    AddLog("[INIT] Initialize: " + std::string(g_Initialize ? "FOUND" : "NOT FOUND"));
    AddLog("[INIT] FindRobloxProcess: " + std::string(g_FindRobloxProcess ? "FOUND" : "NOT FOUND"));
    AddLog("[INIT] Connect: " + std::string(g_Connect ? "FOUND" : "NOT FOUND"));
    AddLog("[INIT] Disconnect: " + std::string(g_Disconnect ? "FOUND" : "NOT FOUND"));
    AddLog("[INIT] GetRobloxPid: " + std::string(g_GetRobloxPid ? "FOUND" : "NOT FOUND"));
    AddLog("[INIT] ExecuteScript: " + std::string(g_ExecuteScript ? "FOUND" : "NOT FOUND"));
    
    // Initialize the API
    if (g_Initialize) {
        bool initResult = g_Initialize();
        AddLog(std::string("[INIT] Initialize result: ") + (initResult ? "SUCCESS" : "FAILED"));
    }
}

// Forward declarations
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void CreateRenderTarget();
void CleanupRenderTarget();
void CleanupDeviceD3D11();

// ImGui UI
char g_scriptBuffer[65536] = "";

void RenderUI() {
    // Get window size and make ImGui fill the entire window
    RECT rect;
    GetClientRect(g_hWnd, &rect);
    ImVec2 size(rect.right - rect.left, rect.bottom - rect.top);
    
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(size);
    
    // No title bar, no resize, no move - fill the window completely
    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | 
                             ImGuiWindowFlags_NoTitleBar | 
                             ImGuiWindowFlags_NoResize | 
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    
    ImGui::Begin("Flora Executor", NULL, flags);
    
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Clear")) memset(g_scriptBuffer, 0, sizeof(g_scriptBuffer));
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings")) {
            ImGui::Checkbox("Always on Top", &g_Settings.alwaysOnTop);
            ImGui::Checkbox("Auto Attach", &g_Settings.autoAttach);
            ImGui::SliderInt("Font Size", &g_Settings.fontSize, 10, 24);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Status
    DWORD attachedPid = g_GetRobloxPid ? g_GetRobloxPid() : 0;
    bool attached = (attachedPid != 0);
    ImGui::TextColored(attached ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1), 
                       attached ? ("Status: Attached (PID: " + std::to_string(attachedPid) + ")").c_str() : "Status: Not Attached");
    
    // Script editor
    ImGui::InputTextMultiline("##script", g_scriptBuffer, sizeof(g_scriptBuffer), 
                              ImVec2(-1, -100), ImGuiInputTextFlags_AllowTabInput);
    
    // Buttons
    if (ImGui::Button("Attach")) {
        if (g_FindRobloxProcess && g_Connect) {
            AddLog("[ATTACH] Finding Roblox process...");
            DWORD pid = g_FindRobloxProcess();
            if (pid == 0) {
                AddLog("[ERROR] No Roblox process found!");
            } else {
                AddLog("[ATTACH] Found Roblox PID: " + std::to_string(pid));
                AddLog("[ATTACH] Connecting...");
                bool connected = g_Connect(pid);
                if (connected) {
                    AddLog("[ATTACH] Successfully connected!");
                } else {
                    AddLog("[ERROR] Failed to connect to Roblox!");
                }
            }
        } else {
            AddLog("[ERROR] API functions not available!");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Detach")) {
        if (g_Disconnect) {
            AddLog("[DETACH] Disconnecting...");
            g_Disconnect();
            AddLog("[DETACH] Disconnected");
        } else {
            AddLog("[ERROR] Disconnect function not available!");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Execute")) {
        if (g_ExecuteScript) {
            AddLog("[EXECUTE] Executing script...");
            int len = strlen(g_scriptBuffer);
            int result = g_ExecuteScript(g_scriptBuffer, len);
            if (result == 0) {
                AddLog("[EXECUTE] Script executed successfully");
            } else {
                AddLog("[ERROR] Execute failed with code: " + std::to_string(result));
            }
        } else {
            AddLog("[ERROR] ExecuteScript function not available!");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        memset(g_scriptBuffer, 0, sizeof(g_scriptBuffer));
        AddLog("[UI] Script editor cleared");
    }
    
    // Log window
    ImGui::Separator();
    ImGui::BeginChild("Logs", ImVec2(0, 0), true);
    for (const auto& log : g_logs) {
        ImGui::TextUnformatted(log.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    
    ImGui::End();
}

std::string GetAppDataPath() {
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        return std::string(appDataPath) + "\\Flora";
    }
    return "";
}

void SaveSettings() {
    std::string appData = GetAppDataPath();
    CreateDirectoryA(appData.c_str(), NULL);
    std::ofstream file(appData + "\\settings.json");
    json settings;
    settings["alwaysOnTop"] = g_Settings.alwaysOnTop;
    settings["autoAttach"] = g_Settings.autoAttach;
    settings["fontSize"] = g_Settings.fontSize;
    file << settings.dump();
    file.close();
}

void LoadSettings() {
    std::string appData = GetAppDataPath();
    std::ifstream file(appData + "\\settings.json");
    if (file.is_open()) {
        try {
            json settings;
            file >> settings;
            g_Settings.alwaysOnTop = settings.value("alwaysOnTop", false);
            g_Settings.autoAttach = settings.value("autoAttach", false);
            g_Settings.fontSize = settings.value("fontSize", 14);
        } catch (...) {}
        file.close();
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    LoadSettings();
    LoadFloraAPI();
    
    WNDCLASSEXA wc = { sizeof(wc) };
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "Flora ImGui";
    RegisterClassExA(&wc);
    
    g_hWnd = CreateWindowExA(0, "Flora ImGui", "Flora Executor", 
                             WS_OVERLAPPEDWINDOW, 100, 100, 1000, 700,
                             NULL, NULL, hInstance, NULL);
    
    // Initialize Direct3D
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevelArray, 2, 
                                        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK) {
        return 1;
    }
    
    CreateRenderTarget();
    
    // Setup ImGui
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;
    
    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    
    ImGui::StyleColorsDark();
    
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    
    // Center window
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    RECT rect;
    GetWindowRect(g_hWnd, &rect);
    int x = (screenWidth - (rect.right - rect.left)) / 2;
    int y = (screenHeight - (rect.bottom - rect.top)) / 2;
    SetWindowPos(g_hWnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    if (g_Settings.alwaysOnTop) {
        SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    
    // Main loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
        if (!IsIconic(g_hWnd)) {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            
            RenderUI();
            
            ImGui::Render();
            const float clear_color_with_alpha[4] = { 0.1f, 0.1f, 0.12f, 1.0f };
            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            g_pSwapChain->Present(1, 0);
        }
    }
    
    SaveSettings();
    
    CleanupDeviceD3D11();
    
    if (g_FloraAPI) FreeLibrary(g_FloraAPI);
    
    return 0;
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }
    
    switch (msg) {
        case WM_SIZE:
            if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) {
                return 0;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = NULL;
    }
}

void CleanupDeviceD3D11() {
    CleanupRenderTarget();
    if (g_pSwapChain) {
        g_pSwapChain->Release();
        g_pSwapChain = NULL;
    }
    if (g_pd3dDeviceContext) {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = NULL;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = NULL;
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
