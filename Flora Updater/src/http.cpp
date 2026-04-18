#include "http.h"
#include <iostream>
#include <vector>

std::string DownloadString(const std::string& url) {
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    
    // Parse URL
    std::string host, path;
    size_t protoEnd = url.find("://");
    if (protoEnd == std::string::npos) return "";
    
    size_t hostStart = protoEnd + 3;
    size_t pathStart = url.find("/", hostStart);
    
    if (pathStart == std::string::npos) {
        host = url.substr(hostStart);
        path = "/";
    } else {
        host = url.substr(hostStart, pathStart - hostStart);
        path = url.substr(pathStart);
    }
    
    // Remove port if present
    size_t portPos = host.find(":");
    if (portPos != std::string::npos) {
        host = host.substr(0, portPos);
    }
    
    hSession = WinHttpOpen(L"FloraUpdater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";
    
    hConnect = WinHttpConnect(hSession, std::wstring(host.begin(), host.end()).c_str(),
                             INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    hRequest = WinHttpOpenRequest(hConnect, L"GET", std::wstring(path.begin(), path.end()).c_str(),
                                  NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                  WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                       WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    
    if (statusCode != 200) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    std::string result;
    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;
    
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1);
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            buffer[bytesRead] = '\0';
            result += buffer.data();
        }
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return result;
}

bool DownloadFile(const std::string& url, const std::string& outputPath) {
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    
    // Parse URL
    std::string host, path;
    size_t protoEnd = url.find("://");
    if (protoEnd == std::string::npos) return false;
    
    size_t hostStart = protoEnd + 3;
    size_t pathStart = url.find("/", hostStart);
    
    if (pathStart == std::string::npos) {
        host = url.substr(hostStart);
        path = "/";
    } else {
        host = url.substr(hostStart, pathStart - hostStart);
        path = url.substr(pathStart);
    }
    
    // Remove port if present
    size_t portPos = host.find(":");
    if (portPos != std::string::npos) {
        host = host.substr(0, portPos);
    }
    
    hSession = WinHttpOpen(L"FloraUpdater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    
    hConnect = WinHttpConnect(hSession, std::wstring(host.begin(), host.end()).c_str(),
                             INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    hRequest = WinHttpOpenRequest(hConnect, L"GET", std::wstring(path.begin(), path.end()).c_str(),
                                  NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                  WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                       WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    
    if (statusCode != 200) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // Open file for binary writing
    FILE* f = nullptr;
    fopen_s(&f, outputPath.c_str(), "wb");
    if (!f) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // Download and write directly to file
    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;
    
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable);
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            fwrite(buffer.data(), 1, bytesRead, f);
        }
    }
    
    fclose(f);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return true;
}
