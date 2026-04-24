#include <windows.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <shlobj.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// Link libraries
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "kernel32.lib")

// Console control handler - prevents app from closing when console window is closed
BOOL WINAPI ConsoleHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_CLOSE_EVENT:
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        // Just free the console without closing the main app
        FreeConsole();
        return TRUE; // Return TRUE to indicate we handled it (don't pass to default handler)
    }
    return FALSE;
}

// Flora API function pointers
typedef bool (__stdcall *Initialize_t)();
typedef unsigned int (__stdcall *FindRobloxProcess_t)();
typedef bool (__stdcall *Connect_t)(unsigned int pid);
typedef void (__stdcall *Disconnect_t)();
typedef unsigned int (__stdcall *GetRobloxPid_t)();
typedef void (__stdcall *RedirConsole_t)();
typedef uintptr_t (__stdcall *GetDataModel_t)();
typedef int (__stdcall *ExecuteScript_t)(const char* source, int sourceLen);
typedef int (__stdcall *GetLastExecError_t)(char* buffer, int bufLen);

// Flora API handles
HMODULE g_FloraDLL = nullptr;
Initialize_t g_Initialize = nullptr;
FindRobloxProcess_t g_FindRobloxProcess = nullptr;
Connect_t g_Connect = nullptr;
Disconnect_t g_Disconnect = nullptr;
GetRobloxPid_t g_GetRobloxPid = nullptr;
RedirConsole_t g_RedirConsole = nullptr;
GetDataModel_t g_GetDataModel = nullptr;
ExecuteScript_t g_ExecuteScript = nullptr;
GetLastExecError_t g_GetLastExecError = nullptr;

// State
bool g_Attached = false;
unsigned int g_RobloxPid = 0;
char g_StatusMessage[256] = "Not attached";

// Log terminal
std::vector<std::string> g_LogMessages;
bool g_AutoScroll = true;

// Auto-search for Roblox
bool g_RobloxFoundLogged = false;

// Tab system
struct ScriptTab {
    std::string name;
    char content[65536];
    bool modified = false;
    ImVec2 scroll = ImVec2(0, 0);
    int cursorPos = 0;
    int selectionStart = 0;
    int selectionEnd = 0;

    ScriptTab() {
        memset(content, 0, sizeof(content));
    }
};

std::vector<ScriptTab> g_Tabs;
int g_ActiveTab = 0;
int g_RenameTab = -1;
char g_RenameBuffer[256] = "";

// Forward declarations
bool LoadFloraAPI();
void UnloadFloraAPI();
void AttachToRoblox();
void DetachFromRoblox();
void ExecuteScript();
void AddLog(const char* message);
void AddNewTab();
void CloseTab(int index);
void SaveTabsToFile();
void LoadTabsFromFile();
void HandleHotkeys();
void RenumberTabs();
std::string GetFloraDataDirectory();

// Helper to initialize default tab
void InitializeTabs() {
    LoadTabsFromFile();
    if (g_Tabs.empty()) {
        ScriptTab tab;
        tab.name = "Script 1";
        strncpy_s(tab.content, "-- Flora Executor\nprint('Hello from Flora!')", sizeof(tab.content));
        tab.modified = false;
        g_Tabs.push_back(tab);
        g_ActiveTab = 0;
    }
    RenumberTabs();
}

void RenumberTabs() {
    int nextScriptNum = 1;
    for (int i = 0; i < g_Tabs.size(); i++) {
        // Check if tab has a custom name (not "Script X" pattern)
        bool isDefaultName = false;
        if (g_Tabs[i].name.find("Script ") == 0) {
            std::string numPart = g_Tabs[i].name.substr(7);
            // Check if the rest is just a number
            bool allDigits = true;
            for (char c : numPart) {
                if (!isdigit(c)) {
                    allDigits = false;
                    break;
                }
            }
            if (allDigits && !numPart.empty()) {
                isDefaultName = true;
            }
        }

        // Only renumber if it's a default name
        if (isDefaultName) {
            g_Tabs[i].name = "Script " + std::to_string(nextScriptNum++);
        }
    }
}

std::string GetFloraDataDirectory() {
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        std::string floraDir = std::string(appDataPath) + "\\Flora";
        // Create directory if it doesn't exist
        CreateDirectoryA(floraDir.c_str(), NULL);
        return floraDir;
    }
    // Fallback to temp directory if AppData fails
    char tempPath[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempPath)) {
        std::string floraDir = std::string(tempPath) + "Flora";
        CreateDirectoryA(floraDir.c_str(), NULL);
        return floraDir;
    }
    // Final fallback to current directory
    return ".";
}

void SaveTabsToFile() {
    std::string floraDir = GetFloraDataDirectory();
    std::string filePath = floraDir + "\\tabs.dat";

    FILE* f = nullptr;
    fopen_s(&f, filePath.c_str(), "wb");
    if (!f) return;

    int tabCount = g_Tabs.size();
    fwrite(&tabCount, sizeof(int), 1, f);
    fwrite(&g_ActiveTab, sizeof(int), 1, f);

    for (const auto& tab : g_Tabs) {
        int nameLen = tab.name.size();
        fwrite(&nameLen, sizeof(int), 1, f);
        fwrite(tab.name.c_str(), nameLen, 1, f);
        fwrite(tab.content, sizeof(tab.content), 1, f);
        fwrite(&tab.modified, sizeof(bool), 1, f);
        fwrite(&tab.scroll, sizeof(ImVec2), 1, f);
        fwrite(&tab.cursorPos, sizeof(int), 1, f);
        fwrite(&tab.selectionStart, sizeof(int), 1, f);
        fwrite(&tab.selectionEnd, sizeof(int), 1, f);
    }

    fclose(f);
}

void LoadTabsFromFile() {
    std::string floraDir = GetFloraDataDirectory();
    std::string filePath = floraDir + "\\tabs.dat";

    FILE* f = nullptr;
    fopen_s(&f, filePath.c_str(), "rb");
    if (!f) return;

    int tabCount = 0;
    fread(&tabCount, sizeof(int), 1, f);
    fread(&g_ActiveTab, sizeof(int), 1, f);

    g_Tabs.clear();
    for (int i = 0; i < tabCount; i++) {
        ScriptTab tab;
        int nameLen = 0;
        fread(&nameLen, sizeof(int), 1, f);
        tab.name.resize(nameLen);
        fread(&tab.name[0], nameLen, 1, f);
        fread(tab.content, sizeof(tab.content), 1, f);
        fread(&tab.modified, sizeof(bool), 1, f);
        fread(&tab.scroll, sizeof(ImVec2), 1, f);
        fread(&tab.cursorPos, sizeof(int), 1, f);
        fread(&tab.selectionStart, sizeof(int), 1, f);
        fread(&tab.selectionEnd, sizeof(int), 1, f);
        g_Tabs.push_back(tab);
    }

    fclose(f);
}

void HandleHotkeys() {
    // Hotkeys: Ctrl+T for new tab, Ctrl+W for close tab, Ctrl+S for save
    if (ImGui::GetIO().KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_T)) {
            AddNewTab();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_W)) {
            CloseTab(g_ActiveTab);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_S)) {
            SaveTabsToFile();
            AddLog("Tabs saved");
        }
    }
}

void AddNewTab() {
    ScriptTab tab;
    strncpy_s(tab.content, "-- Flora Executor\n", sizeof(tab.content));
    tab.modified = false;
    g_Tabs.push_back(tab);
    g_ActiveTab = g_Tabs.size() - 1;
    RenumberTabs();
    SaveTabsToFile();
}

void CloseTab(int index) {
    if (g_Tabs.size() <= 1) return; // Don't close the last tab

    g_Tabs.erase(g_Tabs.begin() + index);

    // Adjust active tab if needed
    if (g_ActiveTab >= g_Tabs.size()) {
        g_ActiveTab = g_Tabs.size() - 1;
    } else if (g_ActiveTab >= index) {
        g_ActiveTab--;
    }
    RenumberTabs();
    SaveTabsToFile();
}

// Data
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Resize handling
static int g_ResizeWidth = 0, g_ResizeHeight = 0;

// Main code
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Register console handler to prevent app from closing when console is closed
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    // Load Flora API
    if (!LoadFloraAPI()) {
        MessageBoxA(nullptr, "Failed to load FloraAPI.dll", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Initialize tabs
    InitializeTabs();

    // Create application window
    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),
        CS_CLASSDC,
        WndProc,
        0L,
        0L,
        hInstance,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        _T("Flora Executor"),
        nullptr
    };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Flora Executor"), WS_OVERLAPPEDWINDOW, 100, 100, 900, 600, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Our state
    ImVec4 clear_color = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    // Main loop
    bool done = false;
    int autoSearchTimer = 0;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window resize
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Auto-search for Roblox process every 60 frames (approx 1 second)
        if (!g_Attached && g_FindRobloxProcess && !g_RobloxFoundLogged) {
            autoSearchTimer++;
            if (autoSearchTimer >= 60) {
                autoSearchTimer = 0;
                DWORD pid = g_FindRobloxProcess();
                if (pid > 0) {
                    AddLog("Roblox Process Found! Ready for attach.");
                    g_RobloxFoundLogged = true;
                }
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Main window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Flora Executor", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse);

        // Title
        ImGui::Text("Flora Executor");
        ImGui::SameLine();
        ImGui::TextDisabled("(Version: 1.0.0)");
        ImGui::Separator();

        // Buttons
        if (!g_Attached) {
            if (ImGui::Button("Attach")) {
                AttachToRoblox();
            }
        } else {
            if (ImGui::Button("Detach")) {
                DetachFromRoblox();
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Clear")) {
            if (g_ActiveTab >= 0 && g_ActiveTab < g_Tabs.size()) {
                memset(g_Tabs[g_ActiveTab].content, 0, sizeof(g_Tabs[g_ActiveTab].content));
                g_Tabs[g_ActiveTab].modified = false;
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Execute")) {
            ExecuteScript();
        }

        ImGui::SameLine();

        if (ImGui::Button("Redir Console")) {
            if (g_RedirConsole) g_RedirConsole();
        }

        ImGui::Separator();

        // Handle hotkeys
        HandleHotkeys();

        // Tab bar
        if (ImGui::BeginTabBar("ScriptTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_TabListPopupButton)) {
            for (int i = 0; i < g_Tabs.size(); i++) {
                ImGui::PushID(i);
                
                std::string tabLabel = g_Tabs[i].name;
                if (g_Tabs[i].modified) tabLabel += " *";

                bool tabOpen = true;
                if (ImGui::BeginTabItem(tabLabel.c_str(), &tabOpen, ImGuiTabItemFlags_None)) {
                    if (g_ActiveTab != i) {
                        g_ActiveTab = i;
                    }
                    // Double-click to rename
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        g_RenameTab = i;
                        strncpy_s(g_RenameBuffer, g_Tabs[i].name.c_str(), sizeof(g_RenameBuffer));
                    }
                    ImGui::EndTabItem();
                }

                // Right-click context menu
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) {
                        g_RenameTab = i;
                        strncpy_s(g_RenameBuffer, g_Tabs[i].name.c_str(), sizeof(g_RenameBuffer));
                    }
                    if (ImGui::MenuItem("Close")) {
                        CloseTab(i);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Duplicate")) {
                        ScriptTab newTab = g_Tabs[i];
                        newTab.name = g_Tabs[i].name + " (Copy)";
                        newTab.modified = true;
                        g_Tabs.push_back(newTab);
                        g_ActiveTab = g_Tabs.size() - 1;
                        RenumberTabs();
                        SaveTabsToFile();
                    }
                    ImGui::EndPopup();
                }

                if (!tabOpen) {
                    CloseTab(i);
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            ImGui::EndTabBar();
        }

        // Show rename input if a tab is being renamed
        if (g_RenameTab >= 0 && g_RenameTab < g_Tabs.size()) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150);
            ImGui::InputText("##Rename", g_RenameBuffer, sizeof(g_RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
            
            // Handle rename completion
            if (ImGui::IsItemDeactivated() || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    g_RenameTab = -1;
                } else if (g_RenameTab >= 0 && g_RenameTab < g_Tabs.size()) {
                    g_Tabs[g_RenameTab].name = g_RenameBuffer;
                    g_RenameTab = -1;
                    SaveTabsToFile();
                }
            }
            if ((ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) && g_RenameTab >= 0 && g_RenameTab < g_Tabs.size()) {
                g_Tabs[g_RenameTab].name = g_RenameBuffer;
                g_RenameTab = -1;
                SaveTabsToFile();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("+")) {
            AddNewTab();
        }
        ImGui::Separator();

        // Script editor with unique ID per tab
        if (g_ActiveTab >= 0 && g_ActiveTab < g_Tabs.size()) {
            char editorID[64];
            sprintf_s(editorID, "##ScriptEditor_%d", g_ActiveTab);
            if (ImGui::InputTextMultiline(editorID, g_Tabs[g_ActiveTab].content, sizeof(g_Tabs[g_ActiveTab].content),
                ImVec2(-FLT_MIN, -200),
                ImGuiInputTextFlags_AllowTabInput)) {
                g_Tabs[g_ActiveTab].modified = true;
                SaveTabsToFile();
            }
        }

        ImGui::Separator();

        // Log terminal
        if (ImGui::BeginChild("LogTerminal", ImVec2(0, 0), true, ImGuiWindowFlags_None)) {
            // Title bar with buttons
            ImGui::Text("Log Terminal");
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                g_LogMessages.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Copy")) {
                std::string allLogs;
                for (const auto& msg : g_LogMessages) {
                    allLogs += msg + "\n";
                }
                if (!allLogs.empty()) {
                    ImGui::SetClipboardText(allLogs.c_str());
                }
            }
            ImGui::Separator();

            // Log content
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (const auto& msg : g_LogMessages) {
                ImGui::TextUnformatted(msg.c_str());
            }
            ImGui::PopStyleVar();

            // Auto-scroll to bottom
            if (g_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        ImGui::End();

        // Rendering
        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    UnloadFloraAPI();

    return 0;
}

// Helper functions
bool CreateDeviceD3D(HWND hWnd)
{
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
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

// Flora API functions
bool LoadFloraAPI() {
    g_FloraDLL = LoadLibraryA("FloraAPI.dll");
    if (!g_FloraDLL) {
        AddLog("Failed: FloraAPI.dll not found");
        return false;
    }

    char dllPath[MAX_PATH];
    GetModuleFileNameA(g_FloraDLL, dllPath, MAX_PATH);

    g_Initialize = (Initialize_t)GetProcAddress(g_FloraDLL, "Initialize");
    g_FindRobloxProcess = (FindRobloxProcess_t)GetProcAddress(g_FloraDLL, "FindRobloxProcess");
    g_Connect = (Connect_t)GetProcAddress(g_FloraDLL, "Connect");
    g_Disconnect = (Disconnect_t)GetProcAddress(g_FloraDLL, "Disconnect");
    g_GetRobloxPid = (GetRobloxPid_t)GetProcAddress(g_FloraDLL, "GetRobloxPid");
    g_RedirConsole = (RedirConsole_t)GetProcAddress(g_FloraDLL, "RedirConsole");
    g_GetDataModel = (GetDataModel_t)GetProcAddress(g_FloraDLL, "GetDataModel");
    g_ExecuteScript = (ExecuteScript_t)GetProcAddress(g_FloraDLL, "ExecuteScript");
    g_GetLastExecError = (GetLastExecError_t)GetProcAddress(g_FloraDLL, "GetLastExecError");

    bool success = g_Initialize && g_FindRobloxProcess && g_Connect && g_Disconnect &&
           g_GetRobloxPid && g_RedirConsole && g_GetDataModel && g_ExecuteScript;
    
    if (success) {
        char logMsg[MAX_PATH + 64];
        sprintf_s(logMsg, "Flora API DLL Found! Location: %s", dllPath);
        AddLog(logMsg);
        AddLog("System Ready");
    } else {
        AddLog("Failed: FloraAPI.dll exports not found");
    }
    
    return success;
}

void UnloadFloraAPI() {
    if (g_FloraDLL) {
        FreeLibrary(g_FloraDLL);
        g_FloraDLL = nullptr;
    }
}

void AttachToRoblox() {
    if (!g_Initialize || !g_FindRobloxProcess || !g_Connect) return;

    if (!g_Initialize()) {
        strcpy_s(g_StatusMessage, "Initialize failed");
        AddLog("Attached Failed: Initialize failed");
        return;
    }

    g_RobloxPid = g_FindRobloxProcess();
    if (!g_RobloxPid) {
        strcpy_s(g_StatusMessage, "Roblox not found");
        AddLog("Attached Failed: Roblox not found");
        return;
    }

    if (g_Connect(g_RobloxPid)) {
        g_Attached = true;
        sprintf_s(g_StatusMessage, "Attached to PID %u", g_RobloxPid);
        AddLog("Successfully Attached to game!");
        AddLog("Successfully Attached. Enjoy!");
    } else {
        strcpy_s(g_StatusMessage, "Connection failed");
        AddLog("Attached Failed: Connection failed");
    }
}

void DetachFromRoblox() {
    if (g_Disconnect) {
        g_Disconnect();
    }
    g_Attached = false;
    g_RobloxPid = 0;
    strcpy_s(g_StatusMessage, "Detached");
    AddLog("Detached from Roblox");
    g_RobloxFoundLogged = false;
}

void ExecuteScript() {
    if (!g_Attached) {
        strcpy_s(g_StatusMessage, "Not attached!");
        AddLog("Execute Failed: Not attached to Roblox");
        return;
    }

    if (!g_ExecuteScript || !g_GetDataModel) {
        strcpy_s(g_StatusMessage, "API not ready");
        AddLog("Execute Failed: API not ready");
        return;
    }

    // Ensure DataModel is found
    if (g_GetDataModel() == 0) {
        strcpy_s(g_StatusMessage, "DataModel not found");
        AddLog("Execute Failed: DataModel not found");
        return;
    }

    // Get content from active tab
    if (g_ActiveTab < 0 || g_ActiveTab >= g_Tabs.size()) {
        strcpy_s(g_StatusMessage, "No active tab");
        AddLog("Execute Failed: No active tab");
        return;
    }

    const char* scriptContent = g_Tabs[g_ActiveTab].content;
    int len = (int)strlen(scriptContent);
    int result = g_ExecuteScript(scriptContent, len);

    if (result == 0) {
        strcpy_s(g_StatusMessage, "Script executed successfully");
        AddLog("Script executed successfully");
        g_Tabs[g_ActiveTab].modified = false;
    } else {
        char errorBuf[1024] = {};
        if (g_GetLastExecError) {
            g_GetLastExecError(errorBuf, sizeof(errorBuf));
        }
        sprintf_s(g_StatusMessage, "Error %d: %s", result, errorBuf);
        AddLog(g_StatusMessage);
    }
}

void AddLog(const char* message) {
    // Get current time for timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    char timestamp[32];
    sprintf_s(timestamp, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    
    std::string logEntry = timestamp + std::string(message);
    g_LogMessages.push_back(logEntry);
}
