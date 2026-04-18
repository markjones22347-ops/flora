#include "zip.h"
#include <windows.h>
#include <iostream>

bool ExtractZip(const std::string& zipPath, const std::string& extractPath) {
    // Use Windows tar.exe (built into Windows 10+) to extract ZIP
    std::string tarCommand = "tar -xf \"" + zipPath + "\" -C \"" + extractPath + "\"";
    
    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {};
    
    if (!CreateProcessA(NULL, const_cast<LPSTR>(tarCommand.c_str()), NULL, NULL, FALSE, 
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return false;
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return exitCode == 0;
}
