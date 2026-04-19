#include "json.h"
#include <sstream>
#include <algorithm>

// Simple JSON parser - only handles the specific format we need
bool ParseUpdateJSON(const std::string& json, UpdateInfo& info) {
    size_t versionPos = json.find("\"version\"");
    if (versionPos == std::string::npos) return false;
    
    size_t versionColon = json.find(":", versionPos);
    if (versionColon == std::string::npos) return false;
    
    size_t versionQuote1 = json.find("\"", versionColon);
    if (versionQuote1 == std::string::npos) return false;
    
    size_t versionQuote2 = json.find("\"", versionQuote1 + 1);
    if (versionQuote2 == std::string::npos) return false;
    
    info.version = json.substr(versionQuote1 + 1, versionQuote2 - versionQuote1 - 1);
    
    size_t urlPos = json.find("\"url\"");
    if (urlPos == std::string::npos) return false;
    
    size_t urlColon = json.find(":", urlPos);
    if (urlColon == std::string::npos) return false;
    
    size_t urlQuote1 = json.find("\"", urlColon);
    if (urlQuote1 == std::string::npos) return false;
    
    size_t urlQuote2 = json.find("\"", urlQuote1 + 1);
    if (urlQuote2 == std::string::npos) return false;
    
    info.url = json.substr(urlQuote1 + 1, urlQuote2 - urlQuote1 - 1);
    
    return !info.version.empty() && !info.url.empty();
}
