#pragma once
#include <string>
#include "json.h"

class Updater {
public:
    Updater(const std::string& customPath = "") : m_customPath(customPath) {}
    virtual ~Updater() = default;
    bool Run();
    
protected:
    virtual void Log(const std::string& message);
    
public:
    std::string GetRemoteVersion();
    void SetLocalVersionWithPath(const std::string& version, const std::string& path);
    
private:
    std::string GetFloraInstallPath();
    std::string GetLocalVersion();
    void SetLocalVersion(const std::string& version);
    bool IsUpdateAvailable(const std::string& localVersion, const UpdateInfo& remoteInfo);
    bool PerformUpdate(const UpdateInfo& info);
    bool InstallFresh(const UpdateInfo& info);
    bool CloseFloraProcess();
    
    std::string m_customPath;
};
