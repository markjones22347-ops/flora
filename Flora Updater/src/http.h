#pragma once
#include <string>
#include <windows.h>
#include <winhttp.h>

bool DownloadFile(const std::string& url, const std::string& outputPath);
std::string DownloadString(const std::string& url);
