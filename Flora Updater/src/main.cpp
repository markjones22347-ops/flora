#include "updater.h"
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>

// Console color codes
enum ConsoleColor {
    COLOR_BLUE = 9,
    COLOR_GREEN = 10,
    COLOR_RED = 12,
    COLOR_WHITE = 15
};

void SetConsoleColor(ConsoleColor color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

void ResetConsoleColor() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, COLOR_WHITE);
}

void LogColored(const std::string& message, ConsoleColor color) {
    SetConsoleColor(color);
    std::cout << message << std::endl;
    ResetConsoleColor();
}

std::string BrowseForFolder() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    
    IFileOpenDialog* pFileOpen = NULL;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, (LPVOID*)&pFileOpen);
    if (FAILED(hr)) {
        CoUninitialize();
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
        CoUninitialize();
        return "";
    }
    
    // Get selected folder
    IShellItem* pItem = NULL;
    hr = pFileOpen->GetResult(&pItem);
    if (FAILED(hr)) {
        pFileOpen->Release();
        CoUninitialize();
        return "";
    }
    
    PWSTR pszPath = NULL;
    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
    if (FAILED(hr)) {
        pItem->Release();
        pFileOpen->Release();
        CoUninitialize();
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
    CoUninitialize();
    
    return path;
}

std::string GetConfigPath() {
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        return std::string(appDataPath) + "\\Flora\\updater_config.txt";
    }
    return "";
}

std::string LoadConfigPath() {
    std::string configPath = GetConfigPath();
    std::ifstream file(configPath);
    if (file.is_open()) {
        std::string path;
        std::getline(file, path);
        file.close();
        return path;
    }
    return "";
}

void SaveConfigPath(const std::string& path) {
    std::string configPath = GetConfigPath();
    std::string configDir = configPath.substr(0, configPath.find_last_of("\\"));
    CreateDirectoryA(configDir.c_str(), NULL);
    
    std::ofstream file(configPath);
    if (file.is_open()) {
        file << path;
        file.close();
    }
}

std::string GetCurrentDirectory() {
    char buffer[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, buffer);
    return std::string(buffer);
}

class ConsoleUpdater : public Updater {
public:
    ConsoleUpdater(const std::string& customPath = "") : Updater(customPath) {}
    
protected:
    void Log(const std::string& message) override {
        std::string lowerMsg = message;
        for (char& c : lowerMsg) c = tolower(c);
        
        if (lowerMsg.find("[failed]") != std::string::npos || lowerMsg.find("[error]") != std::string::npos) {
            LogColored(message, COLOR_RED);
        } else if (lowerMsg.find("[success]") != std::string::npos || lowerMsg.find("[download]") != std::string::npos) {
            LogColored(message, COLOR_GREEN);
        } else if (lowerMsg.find("[boot]") != std::string::npos || lowerMsg.find("[update]") != std::string::npos) {
            LogColored(message, COLOR_BLUE);
        } else {
            std::cout << message << std::endl;
        }
    }
};

void ShowBanner() {
    LogColored(R"(
 ________ ___       ________  ________  ________     
|\  _____\\  \     |\   __  \|\   __  \|\   __  \    
\ \  \__/\ \  \    \ \  \|\  \ \  \|\  \ \  \|\  \   
 \ \   __\\ \  \    \ \  \\\  \ \   _  _\ \   __  \  
  \ \  \_| \ \  \____\ \  \\\  \ \  \\  \\ \  \ \  \ 
   \ \__\   \ \_______\ \_______\ \__\\ _\\ \__\ \__\
    \|__|    \|_______|\|_______|\|__|\|__|\|__|\|__|
                                                     
                                                     
)", COLOR_BLUE);
}

void ShowMenu() {
    std::cout << "\nOptions:\n";
    std::cout << "1) Choose directory to download Flora\n";
    std::cout << "2) Download Flora to current directory\n";
    std::cout << "3) Exit\n";
    std::cout << "\nChoice: ";
}

int main() {
    // Set console code page to 437 for proper ASCII display
    system("chcp 437 > nul");
    
    // Set console title
    SetConsoleTitleA("Flora Updater");
    
    // Enable ANSI colors
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    
    ShowBanner();
    
    // Load saved config path
    std::string savedPath = LoadConfigPath();
    std::string installPath;
    
    // Auto-update check
    std::string currentVersionPath = savedPath.empty() ? GetCurrentDirectory() : savedPath;
    std::string floraExecutorPath = currentVersionPath + "\\Flora Executor";
    std::string versionPath = floraExecutorPath + "\\version.txt";
    
    std::string localVersion = "0.0.0";
    std::ifstream versionFile(versionPath);
    if (versionFile.is_open()) {
        std::getline(versionFile, localVersion);
        versionFile.close();
    }
    
    LogColored("[BOOT] Starting Flora Updater...", COLOR_BLUE);
    
    // Get install directory
    std::string installDir = savedPath.empty() ? GetCurrentDirectory() : savedPath;
    
    // Check for update
    ConsoleUpdater updater(installDir);
    std::string remoteVersion = updater.GetRemoteVersion();
    
    if (!remoteVersion.empty() && remoteVersion != localVersion) {
        LogColored("[AUTO-UPDATE] Newer version found! [" + remoteVersion + "]", COLOR_GREEN);
        std::cout << "Install now? Y/N: ";
        char choice;
        std::cin >> choice;
        if (choice == 'Y' || choice == 'y') {
            if (savedPath.empty()) {
                LogColored("No saved directory found. Please choose a directory.", COLOR_BLUE);
                std::string chosenPath = BrowseForFolder();
                if (!chosenPath.empty()) {
                    LogColored("Directory Chosen: " + chosenPath, COLOR_GREEN);
                    SaveConfigPath(chosenPath);
                    installPath = chosenPath;
                } else {
                    LogColored("[FAILED] No directory chosen, using current directory", COLOR_RED);
                    installPath = GetCurrentDirectory();
                }
            } else {
                installPath = savedPath;
            }
            goto DO_UPDATE;
        }
    }
    
    // Show menu
    while (true) {
        ShowMenu();
        int choice;
        std::cin >> choice;
        
        switch (choice) {
            case 1: {
                std::string chosenPath = BrowseForFolder();
                if (!chosenPath.empty()) {
                    LogColored("Directory Chosen: " + chosenPath, COLOR_GREEN);
                    SaveConfigPath(chosenPath);
                    installPath = chosenPath;
                    goto DO_UPDATE;
                } else {
                    LogColored("[FAILED] No directory chosen", COLOR_RED);
                }
                break;
            }
            case 2: {
                std::string currentDir = GetCurrentDirectory();
                LogColored("Using current directory: " + currentDir, COLOR_BLUE);
                installPath = currentDir;
                goto DO_UPDATE;
            }
            case 3: {
                LogColored("Exiting...", COLOR_BLUE);
                return 0;
            }
            default: {
                LogColored("[FAILED] Invalid choice", COLOR_RED);
                break;
            }
        }
    }
    
DO_UPDATE:
    if (!installPath.empty()) {
        ConsoleUpdater updater(installPath);
        if (updater.Run()) {
            LogColored("[SUCCESS] Update complete!", COLOR_GREEN);
            std::cout << "\nPress Enter to exit...";
            std::cin.ignore();
            std::cin.get();
        } else {
            LogColored("[FAILED] Update failed", COLOR_RED);
            std::cout << "\nPress Enter to exit...";
            std::cin.ignore();
            std::cin.get();
        }
    }
    
    return 0;
}
