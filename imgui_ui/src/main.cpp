#include <windows.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shlobj.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <wrl.h>
#include <WebView2.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "shlwapi.lib")

using json = nlohmann::json;
using Microsoft::WRL::ComPtr;

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

// Settings
struct Settings {
    bool alwaysOnTop = false;
    float textSize = 14.0f;
    bool autoAttach = false;
} g_Settings;

// Window
HWND g_hMainWindow = nullptr;
ComPtr<ICoreWebView2Controller> g_webView2Controller;
ComPtr<ICoreWebView2> g_webView2;

// Window dragging
bool g_IsDragging = false;
POINT g_DragOffset = {0, 0};

// Forward declarations
bool LoadFloraAPI();
void UnloadFloraAPI();
void AttachToRoblox();
void DetachFromRoblox();
void ExecuteScript(const std::string& script);
void SaveSettings();
void LoadSettings();
std::string GetFloraDataDirectory();
void SendToWebView(const std::string& message);
void HandleWebViewMessage(const std::string& message);

std::string GetFloraDataDirectory() {
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        std::string floraDir = std::string(appDataPath) + "\\Flora";
        CreateDirectoryA(floraDir.c_str(), NULL);
        return floraDir;
    }
    char tempPath[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempPath)) {
        std::string floraDir = std::string(tempPath) + "Flora";
        CreateDirectoryA(floraDir.c_str(), NULL);
        return floraDir;
    }
    return ".";
}

bool LoadFloraAPI() {
    char modulePath[MAX_PATH];
    GetModuleFileNameA(NULL, modulePath, MAX_PATH);
    std::string moduleDir = modulePath;
    size_t lastSlash = moduleDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        moduleDir = moduleDir.substr(0, lastSlash);
    }
    std::string dllPath = moduleDir + "\\FloraAPI.dll";

    g_FloraDLL = LoadLibraryA(dllPath.c_str());
    if (!g_FloraDLL) {
        return false;
    }

    g_Initialize = (Initialize_t)GetProcAddress(g_FloraDLL, "Initialize");
    g_FindRobloxProcess = (FindRobloxProcess_t)GetProcAddress(g_FloraDLL, "FindRobloxProcess");
    g_Connect = (Connect_t)GetProcAddress(g_FloraDLL, "Connect");
    g_Disconnect = (Disconnect_t)GetProcAddress(g_FloraDLL, "Disconnect");
    g_GetRobloxPid = (GetRobloxPid_t)GetProcAddress(g_FloraDLL, "GetRobloxPid");
    g_RedirConsole = (RedirConsole_t)GetProcAddress(g_FloraDLL, "RedirConsole");
    g_GetDataModel = (GetDataModel_t)GetProcAddress(g_FloraDLL, "GetDataModel");
    g_ExecuteScript = (ExecuteScript_t)GetProcAddress(g_FloraDLL, "ExecuteScript");
    g_GetLastExecError = (GetLastExecError_t)GetProcAddress(g_FloraDLL, "GetLastExecError");

    if (!g_Initialize || !g_FindRobloxProcess || !g_Connect || !g_Disconnect || 
        !g_GetRobloxPid || !g_RedirConsole || !g_GetDataModel || !g_ExecuteScript) {
        UnloadFloraAPI();
        return false;
    }

    if (g_Initialize()) {
        return true;
    }

    UnloadFloraAPI();
    return false;
}

void UnloadFloraAPI() {
    if (g_Attached && g_Disconnect) {
        g_Disconnect();
    }
    g_Attached = false;
    g_RobloxPid = 0;

    if (g_FloraDLL) {
        FreeLibrary(g_FloraDLL);
        g_FloraDLL = nullptr;
    }

    g_Initialize = nullptr;
    g_FindRobloxProcess = nullptr;
    g_Connect = nullptr;
    g_Disconnect = nullptr;
    g_GetRobloxPid = nullptr;
    g_RedirConsole = nullptr;
    g_GetDataModel = nullptr;
    g_ExecuteScript = nullptr;
    g_GetLastExecError = nullptr;
}

void AttachToRoblox() {
    if (!g_FloraDLL) {
        if (!LoadFloraAPI()) {
            SendToWebView(R"({"type":"log","level":"error","message":"Failed to load FloraAPI.dll"})");
            return;
        }
    }

    if (g_Attached) {
        SendToWebView(R"({"type":"log","level":"warning","message":"Already attached to Roblox"})");
        return;
    }

    unsigned int pid = g_FindRobloxProcess();
    if (pid == 0) {
        SendToWebView(R"({"type":"log","level":"error","message":"Roblox process not found"})");
        return;
    }

    if (g_Connect(pid)) {
        g_Attached = true;
        g_RobloxPid = pid;
        SendToWebView(R"({"type":"attached"})");
        SendToWebView(R"({"type":"log","level":"success","message":"Successfully attached to Roblox"})");
    } else {
        SendToWebView(R"({"type":"log","level":"error","message":"Failed to connect to Roblox"})");
    }
}

void DetachFromRoblox() {
    if (!g_Attached) {
        return;
    }

    if (g_Disconnect) {
        g_Disconnect();
    }

    g_Attached = false;
    g_RobloxPid = 0;
    SendToWebView(R"({"type":"detached"})");
    SendToWebView(R"({"type":"log","level":"info","message":"Detached from Roblox"})");
}

void ExecuteScript(const std::string& script) {
    if (!g_Attached) {
        SendToWebView(R"({"type":"executeResult","success":false,"error":"Not attached to Roblox"})");
        return;
    }

    if (g_ExecuteScript) {
        int result = g_ExecuteScript(script.c_str(), script.length());
        if (result == 0) {
            SendToWebView(R"({"type":"executeResult","success":true})");
            SendToWebView(R"({"type":"log","level":"success","message":"Script executed successfully"})");
        } else {
            char errorBuffer[256];
            if (g_GetLastExecError) {
                g_GetLastExecError(errorBuffer, sizeof(errorBuffer));
                SendToWebView(R"({"type":"executeResult","success":false,"error":"Execution failed"})");
                SendToWebView(R"({"type":"log","level":"error","message":"Script execution failed"})");
            }
        }
    }
}

void SaveSettings() {
    std::string floraDir = GetFloraDataDirectory();
    std::string filePath = floraDir + "\\settings.dat";

    json j;
    j["alwaysOnTop"] = g_Settings.alwaysOnTop;
    j["textSize"] = g_Settings.textSize;
    j["autoAttach"] = g_Settings.autoAttach;

    std::ofstream file(filePath);
    if (file.is_open()) {
        file << j.dump();
        file.close();
    }
}

void LoadSettings() {
    std::string floraDir = GetFloraDataDirectory();
    std::string filePath = floraDir + "\\settings.dat";

    std::ifstream file(filePath);
    if (file.is_open()) {
        try {
            json j;
            file >> j;
            g_Settings.alwaysOnTop = j.value("alwaysOnTop", false);
            g_Settings.textSize = j.value("textSize", 14.0f);
            g_Settings.autoAttach = j.value("autoAttach", false);
        } catch (const json::exception& e) {
            // Use default values if parsing fails
        }
        file.close();
    }

    // Send settings to web view
    json j;
    j["type"] = "settings";
    j["settings"] = {
        {"alwaysOnTop", g_Settings.alwaysOnTop},
        {"textSize", g_Settings.textSize},
        {"autoAttach", g_Settings.autoAttach}
    };
    SendToWebView(j.dump());
}

void SendToWebView(const std::string& message) {
    if (g_webView2) {
        std::wstring messageW(message.begin(), message.end());
        g_webView2->PostWebMessageAsString(messageW.c_str());
    }
}

void HandleWebViewMessage(const std::string& message) {
    try {
        json j = json::parse(message);
        std::string type = j.value("type", "");

        if (type == "attach") {
            AttachToRoblox();
        } else if (type == "detach") {
            DetachFromRoblox();
        } else if (type == "execute") {
            std::string script = j.value("script", "");
            ExecuteScript(script);
        } else if (type == "redirConsole") {
            if (g_RedirConsole) {
                g_RedirConsole();
                SendToWebView(R"({"type":"log","level":"info","message":"Console redirected"})");
            }
        } else if (type == "setAlwaysOnTop") {
            g_Settings.alwaysOnTop = j.value("value", false);
            SaveSettings();
            if (g_hMainWindow) {
                SetWindowPos(g_hMainWindow, g_Settings.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                           0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }
        } else if (type == "setTextSize") {
            g_Settings.textSize = j.value("value", 14.0f);
            SaveSettings();
        } else if (type == "setAutoAttach") {
            g_Settings.autoAttach = j.value("value", false);
            SaveSettings();
        } else if (type == "saveSettings") {
            SaveSettings();
        } else if (type == "loadSettings") {
            LoadSettings();
        } else if (type == "minimize") {
            if (g_hMainWindow) {
                ShowWindow(g_hMainWindow, SW_MINIMIZE);
            }
        } else if (type == "close") {
            PostQuitMessage(0);
        } else if (type == "openDiscord") {
            ShellExecuteA(NULL, "open", "https://discord.gg/your-discord-invite", NULL, NULL, SW_SHOWNORMAL);
        } else if (type == "moveWindow") {
            int deltaX = j.value("deltaX", 0);
            int deltaY = j.value("deltaY", 0);
            if (g_hMainWindow) {
                RECT rect;
                GetWindowRect(g_hMainWindow, &rect);
                SetWindowPos(g_hMainWindow, NULL, rect.left + deltaX, rect.top + deltaY,
                           0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
        }
    } catch (const json::exception& e) {
        // Handle JSON parse error
    }
}

// WebView2 message handler
class WebViewMessageHandler : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    ICoreWebView2WebMessageReceivedEventHandler> {
public:
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        LPWSTR message = nullptr;
        args->TryGetWebMessageAsString(&message);
        if (message) {
            int len = WideCharToMultiByte(CP_UTF8, 0, message, -1, nullptr, 0, nullptr, nullptr);
            std::string messageStr(len, 0);
            WideCharToMultiByte(CP_UTF8, 0, message, -1, &messageStr[0], len, nullptr, nullptr);
            HandleWebViewMessage(messageStr);
            CoTaskMemFree(message);
        }
        return S_OK;
    }
};

// Window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Register window class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = _T("FloraExecutor");
    ::RegisterClassEx(&wc);

    // Create window
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Flora Executor"), WS_POPUP, 
                             100, 100, 1100, 550, nullptr, nullptr, hInstance, nullptr);
    g_hMainWindow = hwnd;

    if (!hwnd) {
        return 1;
    }

    // Get the path to the web UI files
    char modulePath[MAX_PATH];
    GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    std::string moduleDir = modulePath;
    size_t lastSlash = moduleDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        moduleDir = moduleDir.substr(0, lastSlash);
    }
    std::string webUiPath = moduleDir + "\\web_ui\\index.html";

    // Initialize WebView2 environment
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd, webUiPath](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (SUCCEEDED(result)) {
                    // Create WebView2 controller
                    env->CreateCoreWebView2Controller(hwnd,
                        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [hwnd, webUiPath](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                                if (SUCCEEDED(result)) {
                                    g_webView2Controller = controller;
                                    g_webView2Controller->get_CoreWebView2(&g_webView2);
                                    
                                    // Set up message handler
                                    ComPtr<WebViewMessageHandler> messageHandler = Microsoft::WRL::Make<WebViewMessageHandler>();
                                    EventRegistrationToken token;
                                    g_webView2->add_WebMessageReceived(messageHandler.Get(), &token);

                                    // Load HTML file
                                    std::wstring webUiPathW(webUiPath.begin(), webUiPath.end());
                                    g_webView2->Navigate(webUiPathW.c_str());

                                    // Set bounds
                                    RECT bounds;
                                    GetClientRect(hwnd, &bounds);
                                    g_webView2Controller->put_Bounds(bounds);
                                }
                                return S_OK;
                            }).Get());
                }
                return S_OK;
            }).Get());

    if (FAILED(hr)) {
        MessageBoxA(hwnd, "Failed to initialize WebView2. Please install the WebView2 Runtime.", "Error", MB_OK);
        return 1;
    }

    // Load Flora API
    LoadFloraAPI();

    // Load settings
    LoadSettings();

    // Show window
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    UnloadFloraAPI();
    if (g_webView2Controller) {
        g_webView2Controller->Close();
    }

    return 0;
}
