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
    std::string installPath = GetFloraInstallPath();
    std::string floraExecutorPath = installPath + "\\Flora Executor";
    std::string versionPath = floraExecutorPath + "\\version.txt";
    
    std::ifstream file(versionPath);
    if (file.is_open()) {
        std::string version;
        std::getline(file, version);
        file.close();
        return version;
    }
    
    // Fallback to old location if not found in Flora Executor folder
    versionPath = installPath + "\\version.txt";
    file.open(versionPath);
    if (file.is_open()) {
        std::string version;
        std::getline(file, version);
        file.close();
        return version;
    }
    
    return "0.0.0"; // Fresh install
}

void Updater::SetLocalVersion(const std::string& version) {
    std::string versionPath = GetFloraInstallPath() + "\\version.txt";
    
    std::ofstream file(versionPath);
    if (file.is_open()) {
        file << version;
        file.close();
    }
}

void Updater::SetLocalVersionWithPath(const std::string& version, const std::string& path) {
    std::string versionPath = path + "\\version.txt";
    
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
    Sleep(1000);
    
    std::string installPath = GetFloraInstallPath();
    std::string floraExecutorPath = installPath + "\\Flora Executor";
    CreateDirectoryA(floraExecutorPath.c_str(), NULL);
    
    // Download ZIP
    Log("Downloading from Github..");
    std::string zipPath = installPath + "\\flora.zip";
    if (!DownloadFile(info.url, zipPath)) {
        Log("[FAILED] Failed to download update");
        return false;
    }
    Sleep(1000);
    Log("[DOWNLOAD] Download complete");
    
    // Extract ZIP to temp subfolder
    std::string tempPath = installPath + "\\temp_extract";
    CreateDirectoryA(tempPath.c_str(), NULL);
    Log("Extracting files to: " + tempPath);
    Sleep(1000);
    if (!ExtractZip(zipPath, tempPath)) {
        Log("[FAILED] Failed to extract files");
        return false;
    }
    Log("[SUCCESS] Files extracted successfully");
    Sleep(1000);
    
    // Delete ZIP file after extraction
    DeleteFileA(zipPath.c_str());
    Log("Deleted zip file");
    Sleep(1000);
    
    // Move files from temp to Flora Executor folder
    Log("Moving files to: " + floraExecutorPath);
    Sleep(1000);
    // Move all files from temp to Flora Executor path
    WIN32_FIND_DATAA findData;
    std::string searchPath = tempPath + "\\*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0) {
                std::string srcPath = tempPath + "\\" + findData.cFileName;
                std::string dstPath = floraExecutorPath + "\\" + findData.cFileName;
                MoveFileExA(srcPath.c_str(), dstPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    
    // Remove temp directory
    RemoveDirectoryA(tempPath.c_str());
    Log("Extracted folder: " + floraExecutorPath);
    Sleep(1000);
    
    // Update version (save to Flora Executor folder)
    SetLocalVersionWithPath(info.version, floraExecutorPath);
    
    Log("[SUCCESS]");
    return true;
}

bool Updater::InstallFresh(const UpdateInfo& info) {
    std::string installPath = GetFloraInstallPath();
    std::string floraExecutorPath = installPath + "\\Flora Executor";
    CreateDirectoryA(floraExecutorPath.c_str(), NULL);
    Sleep(1000);
    
    Log("Downloading from Github..");
    std::string zipPath = installPath + "\\flora.zip";
    if (!DownloadFile(info.url, zipPath)) {
        Log("[FAILED] Failed to download initial version");
        return false;
    }
    Sleep(1000);
    Log("[DOWNLOAD] Download complete");
    
    // Extract ZIP to temp subfolder
    std::string tempPath = installPath + "\\temp_extract";
    CreateDirectoryA(tempPath.c_str(), NULL);
    Log("Extracting files to: " + tempPath);
    Sleep(1000);
    if (!ExtractZip(zipPath, tempPath)) {
        Log("[FAILED] Failed to extract files");
        return false;
    }
    Log("[SUCCESS] Files extracted successfully");
    Sleep(1000);
    
    // Delete ZIP file after extraction
    DeleteFileA(zipPath.c_str());
    Log("Deleted zip file");
    Sleep(1000);
    
    // Move files from temp to Flora Executor folder
    Log("Moving files to: " + floraExecutorPath);
    Sleep(1000);
    // Move all files from temp to Flora Executor path
    WIN32_FIND_DATAA findData;
    std::string searchPath = tempPath + "\\*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0) {
                std::string srcPath = tempPath + "\\" + findData.cFileName;
                std::string dstPath = floraExecutorPath + "\\" + findData.cFileName;
                MoveFileExA(srcPath.c_str(), dstPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    
    // Remove temp directory
    RemoveDirectoryA(tempPath.c_str());
    Log("Extracted folder: " + floraExecutorPath);
    Sleep(1000);
    
    // Write version (save to Flora Executor folder)
    SetLocalVersionWithPath(info.version, floraExecutorPath);
    
    Log("[SUCCESS]");
    return true;
}

void Updater::Log(const std::string& message) {
    std::cout << message << std::endl;
}

std::string Updater::GetRemoteVersion() {
    const std::string UPDATE_JSON_URL = "https://raw.githubusercontent.com/markjones22347-ops/flora/main/update.json";
    
    std::string json = DownloadString(UPDATE_JSON_URL);
    if (json.empty()) {
        return "";
    }
    
    UpdateInfo info;
    if (!ParseUpdateJSON(json, info)) {
        return "";
    }
    
    return info.version;
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
    
    // Launch Flora from Flora Executor folder
    std::string installPath = GetFloraInstallPath();
    std::string floraExecutorPath = installPath + "\\Flora Executor";
    std::string floraPath = floraExecutorPath + "\\Flora.exe";
    Log("[BOOT] Launching Newest Version...");
    
    ShellExecuteA(NULL, "open", floraPath.c_str(), NULL, floraExecutorPath.c_str(), SW_SHOW);
    
    Sleep(3000);
    Log("Closing in 3 Seconds...");
    
    return true;
}
