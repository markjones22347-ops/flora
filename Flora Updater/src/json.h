#pragma once
#include <string>
#include <map>

struct UpdateInfo {
    std::string version;
    std::string url;
};

bool ParseUpdateJSON(const std::string& json, UpdateInfo& info);
