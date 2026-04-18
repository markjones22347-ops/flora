#include "updater.h"
#include "http.h"
#include "zip.h"
#include "version.h"
#include <windows.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <filesystem>

const std::string UPDATE_JSON_URL = "https://raw.githubusercontent.com/markjones22347-ops/flora/main/update.json";

std::string Updater::GetFloraInstallPath() {
    if (!m_customPath.empty()) {
        return m_customPath;
    }
    
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        std::string floraDir = std::string(appDataPath) + "\\Flora";
        return floraDir;
    }
    return "";
}

std::string Updater::GetLocalVersion() {
    std::string versionPath = GetFloraInstallPath() + "\\version.txt";
    
    std::ifstream file(versionPath);
    if (!file.is_open()) {
        return "0.0.0"; // Fresh install
    }
    
    std::string version;
    std::getline(file, version);
    file.close();
    
    return version;
}

void Updater::SetLocalVersion(const std::string& version) {
    std::string versionPath = GetFloraInstallPath() + "\\version.txt";
    
    std::ofstream file(versionPath);
    if (file.is_open()) {
        file << version;
        file.close();
    }
}

bool Updater::IsUpdateAvailable(const std::string& localVersion, const UpdateInfo& remoteInfo) {
    return CompareVersions(localVersion, remoteInfo.version) < 0;
}

bool Updater::CloseFloraProcess() {
    // Find and close Flora.exe
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;
    
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(hSnapshot, &pe32)) {
        do {
            std::string processName = pe32.szExeFile;
            if (processName == "Flora.exe") {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                    Sleep(1000); // Wait for process to close
                }
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    return true;
}

bool Updater::PerformUpdate(const UpdateInfo& info) {
    Log("[UPDATE] Found Newer Version! Current Version: " + GetLocalVersion() + " --> Newer Version: " + info.version);
    
    std::string installPath = GetFloraInstallPath();
    
    // Download ZIP
    Log("[DOWNLOAD] Downloading update from: " + info.url);
    std::string zipPath = installPath + "\\flora.zip";
    Log("[DOWNLOAD] Downloading to: " + zipPath);
    if (!DownloadFile(info.url, zipPath)) {
        Log("[FAILED] Failed to download update");
        return false;
    }
    Log("[DOWNLOAD] Download complete");
    
    // Update version
    SetLocalVersion(info.version);
    
    Log("[SUCCESS]");
    return true;
}

bool Updater::InstallFresh(const UpdateInfo& info) {
    std::string installPath = GetFloraInstallPath();
    CreateDirectoryA(installPath.c_str(), NULL);
    
    Log("[DOWNLOAD] Downloading initial version from: " + info.url);
    std::string zipPath = installPath + "\\flora.zip";
    Log("[DOWNLOAD] Downloading to: " + zipPath);
    if (!DownloadFile(info.url, zipPath)) {
        Log("[FAILED] Failed to download initial version");
        return false;
    }
    Log("[DOWNLOAD] Download complete");
    
    // Write version
    SetLocalVersion(info.version);
    
    Log("[SUCCESS]");
    return true;
}

void Updater::Log(const std::string& message) {
    std::cout << message << std::endl;
}

bool Updater::Run() {
    Log("[BOOT] Starting Flora Updater...");
    
    // Get local version
    std::string localVersion = GetLocalVersion();
    
    // Download update.json
    std::string json = DownloadString(UPDATE_JSON_URL);
    if (json.empty()) {
        Log("[FAILED] Failed to download update.json");
        return false;
    }
    
    // Parse JSON
    UpdateInfo info;
    if (!ParseUpdateJSON(json, info)) {
        Log("[FAILED] Failed to parse update.json");
        return false;
    }
    
    // Check if update needed
    if (IsUpdateAvailable(localVersion, info)) {
        // Perform update
        if (localVersion == "0.0.0") {
            // Fresh install
            if (!InstallFresh(info)) {
                return false;
            }
        } else {
            // Update
            if (!PerformUpdate(info)) {
                return false;
            }
        }
    } else {
        Log("[UPDATE] No updates found.");
    }
    
    // Launch Flora
    std::string floraPath = GetFloraInstallPath() + "\\Flora.exe";
    Log("[BOOT] Launching Newest Version...");
    
    ShellExecuteA(NULL, "open", floraPath.c_str(), NULL, GetFloraInstallPath().c_str(), SW_SHOW);
    
    Sleep(3000);
    Log("Closing in 3 Seconds...");
    
    return true;
}
