#include <windows.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <WebView2.h>
#include <wrl.h>
#include <dwmapi.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "dwmapi.lib")

using json = nlohmann::json;
using namespace Microsoft::WRL;

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
    bool autoAttach = false;
    int fontSize = 14;
} g_Settings;

// Script tabs
struct ScriptTab {
    std::string name;
    std::string content;
    bool open = true;
};
std::vector<ScriptTab> g_ScriptTabs;
int g_CurrentTab = 0;

// WebView2
ICoreWebView2* g_webview = nullptr;
ICoreWebView2Controller* g_webviewController = nullptr;
HWND g_hMainWindow = nullptr;

// Window dragging
bool g_IsDragging = false;
POINT g_DragOffset = {0, 0};

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void LoadFloraAPI();
void Inject();
void Execute();
void Disconnect();
void RefreshProcs();
std::vector<unsigned int> GetRobloxProcesses();
std::string GetAppDataPath();
void SaveSettings();
void LoadSettings();

// IPC handlers
void HandleWebMessage(const std::string& message);
void SendToWebView(const std::string& message);

// Helper functions
std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

std::string GetHTMLPath() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string exePath(path);
    size_t lastSlash = exePath.find_last_of("\\");
    if (lastSlash != std::string::npos) {
        exePath = exePath.substr(0, lastSlash);
    }
    // Go up from build/Release to project root, then to web_ui
    size_t buildPos = exePath.find("\\build\\");
    if (buildPos != std::string::npos) {
        exePath = exePath.substr(0, buildPos);
    }
    std::string htmlPath = exePath + "\\web_ui\\index.html";
    return htmlPath;
}

std::string ReadHTMLContent() {
    std::string htmlPath = GetHTMLPath();
    std::ifstream file(htmlPath);
    if (!file.is_open()) {
        return "";
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

void SendToWebView(const std::string& message) {
    if (g_webview) {
        int size = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, NULL, 0);
        if (size > 0) {
            std::wstring wmessage(size, 0);
            MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, &wmessage[0], size);
            g_webview->PostWebMessageAsJson(wmessage.c_str());
        }
    }
}

void HandleWebMessage(const std::string& messageJson) {
    try {
        json msg = json::parse(messageJson);
        std::string action = msg["action"];
        
        if (action == "inject") {
            Inject();
        }
        else if (action == "execute") {
            std::string code = msg["code"];
            if (!code.empty()) {
                if (g_ExecuteScript) {
                    int result = g_ExecuteScript(code.c_str(), code.length());
                    json response;
                    response["action"] = "execResult";
                    response["success"] = (result == 0);
                    if (result != 0) {
                        char error[512];
                        g_GetLastExecError(error, 512);
                        response["error"] = error;
                    }
                    SendToWebView(response.dump());
                }
            }
        }
        else if (action == "disconnect") {
            Disconnect();
        }
        else if (action == "refreshProcs") {
            RefreshProcs();
        }
        else if (action == "killProc") {
            unsigned int pid = msg["pid"];
            HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (hProc) {
                TerminateProcess(hProc, 0);
                CloseHandle(hProc);
            }
            RefreshProcs();
        }
        else if (action == "toggleSetting") {
            std::string key = msg["key"];
            bool value = msg["value"];
            if (key == "alwaysOnTop") g_Settings.alwaysOnTop = value;
            else if (key == "autoAttach") g_Settings.autoAttach = value;
            else if (key == "fontSize") g_Settings.fontSize = msg["size"];
            SaveSettings();
            
            if (key == "alwaysOnTop") {
                SetWindowPos(g_hMainWindow, value ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }
        }
        else if (action == "saveFile") {
            std::string code = msg["code"];
            std::string name = msg["name"];
            std::string appData = GetAppDataPath();
            std::string scriptsPath = appData + "\\scripts";
            CreateDirectoryA(scriptsPath.c_str(), NULL);
            std::ofstream file(scriptsPath + "\\" + name + ".lua");
            file << code;
            file.close();
        }
        else if (action == "openFile") {
            std::string name = msg["name"];
            std::string appData = GetAppDataPath();
            std::string scriptsPath = appData + "\\scripts";
            std::string content = ReadFile(scriptsPath + "\\" + name + ".lua");
            json response;
            response["action"] = "fileLoaded";
            response["name"] = name;
            response["content"] = content;
            SendToWebView(response.dump());
        }
        else if (action == "newTab") {
            std::string name = msg["name"];
            std::string content = msg["content"];
            ScriptTab tab;
            tab.name = name;
            tab.content = content;
            tab.open = true;
            g_ScriptTabs.push_back(tab);
            g_CurrentTab = g_ScriptTabs.size() - 1;
        }
        else if (action == "closeTab") {
            int id = msg["id"];
            if (id >= 0 && id < g_ScriptTabs.size()) {
                g_ScriptTabs.erase(g_ScriptTabs.begin() + id);
                if (g_CurrentTab >= g_ScriptTabs.size()) {
                    g_CurrentTab = g_ScriptTabs.size() - 1;
                }
            }
        }
        else if (action == "switchTab") {
            int id = msg["id"];
            if (id >= 0 && id < g_ScriptTabs.size()) {
                g_CurrentTab = id;
            }
        }
        else if (action == "renameTab") {
            int id = msg["id"];
            std::string name = msg["name"];
            if (id >= 0 && id < g_ScriptTabs.size()) {
                g_ScriptTabs[id].name = name;
            }
        }
        else if (action == "getTabs") {
            json response;
            response["action"] = "tabsData";
            json tabs = json::array();
            for (size_t i = 0; i < g_ScriptTabs.size(); i++) {
                json tab;
                tab["id"] = i;
                tab["name"] = g_ScriptTabs[i].name;
                tab["content"] = g_ScriptTabs[i].content;
                tabs.push_back(tab);
            }
            response["tabs"] = tabs;
            response["currentTab"] = g_CurrentTab;
            SendToWebView(response.dump());
        }
        else if (action == "winClose") {
            PostMessage(g_hMainWindow, WM_CLOSE, 0, 0);
        }
        else if (action == "winMin") {
            ShowWindow(g_hMainWindow, SW_MINIMIZE);
        }
        else if (action == "winMax") {
            static bool maximized = false;
            if (maximized) {
                ShowWindow(g_hMainWindow, SW_RESTORE);
                maximized = false;
            } else {
                ShowWindow(g_hMainWindow, SW_MAXIMIZE);
                maximized = true;
            }
        }
        else if (action == "log") {
            std::string msg_text = msg["message"];
            std::string type = msg["type"];
        }
        else if (action == "getSettings") {
            json response;
            response["action"] = "settingsData";
            response["alwaysOnTop"] = g_Settings.alwaysOnTop;
            response["autoAttach"] = g_Settings.autoAttach;
            response["fontSize"] = g_Settings.fontSize;
            SendToWebView(response.dump());
        }
    } catch (const std::exception& e) {
    }
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
    
    g_FloraDLL = LoadLibraryA(dllPath.c_str());
    if (!g_FloraDLL) {
        return;
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
    
    if (g_Initialize) {
        g_Initialize();
    }
}

void Inject() {
    if (!g_FindRobloxProcess || !g_Connect) return;
    
    unsigned int pid = g_FindRobloxProcess();
    if (pid == 0) {
        json response;
        response["action"] = "injectResult";
        response["success"] = false;
        response["error"] = "Roblox not found";
        SendToWebView(response.dump());
        return;
    }
    
    if (g_Connect(pid)) {
        g_Attached = true;
        g_RobloxPid = pid;
        json response;
        response["action"] = "injectResult";
        response["success"] = true;
        response["pid"] = pid;
        SendToWebView(response.dump());
    } else {
        json response;
        response["action"] = "injectResult";
        response["success"] = false;
        response["error"] = "Failed to attach";
        SendToWebView(response.dump());
    }
}

void Execute() {
    if (!g_Attached || !g_ExecuteScript) return;
}

void Disconnect() {
    if (g_Disconnect) {
        g_Disconnect();
    }
    g_Attached = false;
    g_RobloxPid = 0;
    
    json response;
    response["action"] = "disconnected";
    SendToWebView(response.dump());
}

std::vector<unsigned int> GetRobloxProcesses() {
    std::vector<unsigned int> pids;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return pids;
    
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(hSnapshot, &pe32)) {
        do {
            std::string name = pe32.szExeFile;
            if (name == "RobloxPlayerBeta.exe" || name == "RobloxPlayerLauncher.exe" || 
                name == "Windows10Universal.exe" || name == "Roblox.exe") {
                pids.push_back(pe32.th32ProcessID);
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    return pids;
}

void RefreshProcs() {
    std::vector<unsigned int> pids = GetRobloxProcesses();
    
    json response;
    response["action"] = "procList";
    json procs = json::array();
    for (unsigned int pid : pids) {
        json proc;
        proc["pid"] = pid;
        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (hProc) {
            char name[MAX_PATH];
            if (GetModuleFileNameExA(hProc, NULL, name, MAX_PATH)) {
                proc["name"] = name;
            } else {
                proc["name"] = "Roblox";
            }
            CloseHandle(hProc);
        } else {
            proc["name"] = "Roblox";
        }
        procs.push_back(proc);
    }
    response["processes"] = procs;
    SendToWebView(response.dump());
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

class WebMessageHandler : public ICoreWebView2WebMessageReceivedEventHandler {
public:
    WebMessageHandler() : m_refCount(1) {}
    
    HRESULT STDMETHODCALLTYPE Invoke(
        ICoreWebView2* sender,
        ICoreWebView2WebMessageReceivedEventArgs* args) {
        
        LPWSTR message = nullptr;
        args->get_WebMessageAsJson(&message);
        
        int size = WideCharToMultiByte(CP_UTF8, 0, message, -1, NULL, 0, NULL, NULL);
        if (size > 0) {
            std::string utf8Message(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, message, -1, &utf8Message[0], size, NULL, NULL);
            utf8Message.resize(size - 1);
            HandleWebMessage(utf8Message);
        }
        
        CoTaskMemFree(message);
        return S_OK;
    }
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2WebMessageReceivedEventHandler)) {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    
    ULONG STDMETHODCALLTYPE AddRef() { return InterlockedIncrement(&m_refCount); }
    ULONG STDMETHODCALLTYPE Release() { 
        ULONG ref = InterlockedDecrement(&m_refCount);
        if (ref == 0) delete this;
        return ref;
    }

private:
    LONG m_refCount;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    LoadSettings();
    LoadFloraAPI();
    
    WNDCLASSEXA wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(10, 10, 12));
    wc.lpszClassName = "FloraWebView";
    
    RegisterClassExA(&wc);
    
    g_hMainWindow = CreateWindowExA(
        0,
        "FloraWebView",
        "Flora",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        100, 100, 1000, 700,
        NULL, NULL, hInstance, NULL
    );
    
    // Center window
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    RECT rect;
    GetWindowRect(g_hMainWindow, &rect);
    int x = (screenWidth - (rect.right - rect.left)) / 2;
    int y = (screenHeight - (rect.bottom - rect.top)) / 2;
    SetWindowPos(g_hMainWindow, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    if (g_Settings.alwaysOnTop) {
        SetWindowPos(g_hMainWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    
    // Initialize WebView2 synchronously
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (SUCCEEDED(result)) {
                    env->CreateCoreWebView2Controller(
                        g_hMainWindow,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                                if (SUCCEEDED(result)) {
                                    g_webviewController = controller;
                                    g_webviewController->get_CoreWebView2(&g_webview);
                                    
                                    ICoreWebView2Settings* settings;
                                    g_webview->get_Settings(&settings);
                                    settings->put_IsScriptEnabled(TRUE);
                                    settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                                    settings->put_AreDevToolsEnabled(TRUE);
                                    
                                    RECT bounds;
                                    GetClientRect(g_hMainWindow, &bounds);
                                    g_webviewController->put_Bounds(bounds);
                                    
                                    WebMessageHandler* msgHandler = new WebMessageHandler();
                                    g_webview->add_WebMessageReceived(msgHandler, nullptr);
                                    
                                    // Get absolute path to HTML file
                                    char path[MAX_PATH];
                                    GetModuleFileNameA(NULL, path, MAX_PATH);
                                    std::string exePath(path);
                                    size_t lastSlash = exePath.find_last_of("\\");
                                    if (lastSlash != std::string::npos) {
                                        exePath = exePath.substr(0, lastSlash);
                                    }
                                    
                                    // Go up from build/Release to imgui_ui, then to web_ui
                                    size_t buildPos = exePath.rfind("\\build\\");
                                    if (buildPos != std::string::npos) {
                                        exePath = exePath.substr(0, buildPos);
                                    }
                                    
                                    std::string htmlPath = exePath + "\\web_ui\\index.html";
                                    
                                    // Convert to file:// URL with forward slashes
                                    std::string fileUrl = "file:///";
                                    for (char c : htmlPath) {
                                        if (c == '\\') fileUrl += '/';
                                        else fileUrl += c;
                                    }
                                    
                                    int wsize = MultiByteToWideChar(CP_UTF8, 0, fileUrl.c_str(), -1, NULL, 0);
                                    std::wstring wfileUrl(wsize, 0);
                                    MultiByteToWideChar(CP_UTF8, 0, fileUrl.c_str(), -1, &wfileUrl[0], wsize);
                                    g_webview->Navigate(wfileUrl.c_str());
                                    
                                    json initMsg;
                                    initMsg["action"] = "init";
                                    initMsg["alwaysOnTop"] = g_Settings.alwaysOnTop;
                                    initMsg["autoAttach"] = g_Settings.autoAttach;
                                    initMsg["fontSize"] = g_Settings.fontSize;
                                    SendToWebView(initMsg.dump());
                                    
                                    g_webviewController->put_IsVisible(TRUE);
                                } else {
                                    MessageBoxA(g_hMainWindow, "Failed to create WebView2 controller", "Error", MB_OK | MB_ICONERROR);
                                }
                                return S_OK;
                            }).Get());
                } else {
                    MessageBoxA(g_hMainWindow, "Failed to create WebView2 environment. Make sure WebView2 runtime is installed.", "Error", MB_OK | MB_ICONERROR);
                }
                return S_OK;
            }).Get());
    
    if (FAILED(hr)) {
        MessageBoxA(g_hMainWindow, "Failed to initialize WebView2", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    ShowWindow(g_hMainWindow, nCmdShow);
    UpdateWindow(g_hMainWindow);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    if (g_webviewController) {
        g_webviewController->Close();
        g_webviewController = nullptr;
    }
    if (g_webview) {
        g_webview = nullptr;
    }
    
    if (g_Disconnect) {
        g_Disconnect();
    }
    
    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_SIZE: {
            RECT bounds;
            GetClientRect(hWnd, &bounds);
            if (g_webviewController) {
                g_webviewController->put_Bounds(bounds);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            g_IsDragging = true;
            g_DragOffset.x = LOWORD(lParam);
            g_DragOffset.y = HIWORD(lParam);
            return 0;
        }
        case WM_LBUTTONUP: {
            g_IsDragging = false;
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (g_IsDragging) {
                POINT pt = { LOWORD(lParam), HIWORD(lParam) };
                ClientToScreen(hWnd, &pt);
                SetWindowPos(hWnd, NULL, pt.x - g_DragOffset.x, pt.y - g_DragOffset.y, 0, 0, 
                             SWP_NOSIZE | SWP_NOZORDER);
            }
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}
