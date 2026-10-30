#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct AudioEndpointInfo {
    std::wstring guid;
    std::wstring name;
    std::wstring deviceName;
    bool isApoInstalled;
};

class ApoManager {
public:
    static std::vector<AudioEndpointInfo> EnumerateEndpoints();
    static bool InstallApoToEndpoint(const std::wstring& endpointGuid, const std::wstring& apoClsid = L"{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}");
    static bool UninstallApoFromEndpoint(const std::wstring& endpointGuid);
    static bool InstallApoToAllEndpoints(const std::wstring& apoClsid = L"{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}");
    static bool UninstallApoFromAllEndpoints();

    static bool RegisterApoDll(const std::wstring& dllPath);
    static bool UnregisterApoDll();

    static void RestartAudioService();
};
