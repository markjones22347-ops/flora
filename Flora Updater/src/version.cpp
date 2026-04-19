#include "version.h"
#include <sstream>
#include <algorithm>

// Simple semantic version comparison
// Returns: -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
int CompareVersions(const std::string& v1, const std::string& v2) {
    std::istringstream ss1(v1);
    std::istringstream ss2(v2);
    
    int major1 = 0, minor1 = 0, patch1 = 0;
    int major2 = 0, minor2 = 0, patch2 = 0;
    
    char dot;
    
    ss1 >> major1 >> dot >> minor1 >> dot >> patch1;
    ss2 >> major2 >> dot >> minor2 >> dot >> patch2;
    
    if (major1 != major2) return (major1 < major2) ? -1 : 1;
    if (minor1 != minor2) return (minor1 < minor2) ? -1 : 1;
    if (patch1 != patch2) return (patch1 < patch2) ? -1 : 1;
    
    return 0;
}
