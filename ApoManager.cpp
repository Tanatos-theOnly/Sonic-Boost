#include "ApoManager.h"
#include <iostream>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

static const WCHAR RENDER_BASE_KEY[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Render";
static const WCHAR BACKUP_BASE_KEY[] = L"SOFTWARE\\SonicBoost\\OriginalEndpointEffects";

static const WCHAR PKEY_FX_ENDPOINT[] = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},2";
static const WCHAR PKEY_COMPOSITE_FX_ENDPOINT[] = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},15";

std::vector<AudioEndpointInfo> ApoManager::EnumerateEndpoints() {
    std::vector<AudioEndpointInfo> endpoints;
    HKEY hBase = NULL;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, RENDER_BASE_KEY, 0, KEY_READ, &hBase) != ERROR_SUCCESS) {
        return endpoints;
    }

    DWORD index = 0;
    WCHAR subKeyName[256];
    DWORD nameLen = 256;

    while (RegEnumKeyExW(hBase, index++, subKeyName, &nameLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
        nameLen = 256;
        AudioEndpointInfo info;
        info.guid = subKeyName;
        info.isApoInstalled = false;

        // Read friendly names from Properties
        std::wstring propKeyPath = std::wstring(RENDER_BASE_KEY) + L"\\" + subKeyName + L"\\Properties";
        HKEY hProp = NULL;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, propKeyPath.c_str(), 0, KEY_READ, &hProp) == ERROR_SUCCESS) {
            WCHAR buf[256] = {};
            DWORD cbData = sizeof(buf);
            if (RegQueryValueExW(hProp, L"{b3f8fa53-0004-438e-9003-51a46e139bfc},6", NULL, NULL, (LPBYTE)buf, &cbData) == ERROR_SUCCESS) {
                info.name = buf;
            }
            cbData = sizeof(buf);
            if (RegQueryValueExW(hProp, L"{a45c254e-df1c-4efd-8020-67d146a850e0},2", NULL, NULL, (LPBYTE)buf, &cbData) == ERROR_SUCCESS) {
                info.deviceName = buf;
            }
            RegCloseKey(hProp);
        }

        // Check if our APO is installed in FxProperties
        std::wstring fxKeyPath = std::wstring(RENDER_BASE_KEY) + L"\\" + subKeyName + L"\\FxProperties";
        HKEY hFx = NULL;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, fxKeyPath.c_str(), 0, KEY_READ, &hFx) == ERROR_SUCCESS) {
            WCHAR clsidVal[256] = {};
            DWORD cbData = sizeof(clsidVal);
            if (RegQueryValueExW(hFx, PKEY_FX_ENDPOINT, NULL, NULL, (LPBYTE)clsidVal, &cbData) == ERROR_SUCCESS) {
                if (_wcsicmp(clsidVal, L"{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}") == 0) {
                    info.isApoInstalled = true;
                }
            }
            RegCloseKey(hFx);
        }

        endpoints.push_back(info);
    }

    RegCloseKey(hBase);
    return endpoints;
}

bool ApoManager::InstallApoToEndpoint(const std::wstring& endpointGuid, const std::wstring& apoClsid) {
    std::wstring fxPath = std::wstring(RENDER_BASE_KEY) + L"\\" + endpointGuid + L"\\FxProperties";
    std::wstring backupPath = std::wstring(BACKUP_BASE_KEY) + L"\\" + endpointGuid;

    HKEY hBackup = NULL;
    RegCreateKeyExW(HKEY_LOCAL_MACHINE, backupPath.c_str(), 0, NULL, 0, KEY_WRITE | KEY_READ, NULL, &hBackup, NULL);

    HKEY hFx = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, fxPath.c_str(), 0, KEY_READ | KEY_WRITE, &hFx) != ERROR_SUCCESS) {
        if (hBackup) RegCloseKey(hBackup);
        return false;
    }

    // 1. Backup existing values if not already backed up
    if (hBackup) {
        WCHAR existingClsid[256] = {};
        DWORD cbData = sizeof(existingClsid);
        DWORD type = 0;
        if (RegQueryValueExW(hFx, PKEY_FX_ENDPOINT, NULL, &type, (LPBYTE)existingClsid, &cbData) == ERROR_SUCCESS) {
            if (_wcsicmp(existingClsid, apoClsid.c_str()) != 0) {
                RegSetValueExW(hBackup, PKEY_FX_ENDPOINT, 0, type, (const BYTE*)existingClsid, cbData);
            }
        } else {
            const WCHAR na[] = L"GUID_NA";
            RegSetValueExW(hBackup, PKEY_FX_ENDPOINT, 0, REG_SZ, (const BYTE*)na, sizeof(na));
        }
        RegCloseKey(hBackup);
    }

    // 2. Write new APO CLSID
    DWORD cbData = (DWORD)((apoClsid.length() + 1) * sizeof(WCHAR));
    RegSetValueExW(hFx, PKEY_FX_ENDPOINT, 0, REG_SZ, (const BYTE*)apoClsid.c_str(), cbData);

    // REG_MULTI_SZ requires double null terminator
    std::wstring multiSz = apoClsid + L'\0';
    DWORD cbMulti = (DWORD)((multiSz.length() + 1) * sizeof(WCHAR));
    RegSetValueExW(hFx, PKEY_COMPOSITE_FX_ENDPOINT, 0, REG_MULTI_SZ, (const BYTE*)multiSz.c_str(), cbMulti);

    RegCloseKey(hFx);
    return true;
}

bool ApoManager::UninstallApoFromEndpoint(const std::wstring& endpointGuid) {
    std::wstring fxPath = std::wstring(RENDER_BASE_KEY) + L"\\" + endpointGuid + L"\\FxProperties";
    std::wstring backupPath = std::wstring(BACKUP_BASE_KEY) + L"\\" + endpointGuid;

    HKEY hFx = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, fxPath.c_str(), 0, KEY_READ | KEY_WRITE, &hFx) != ERROR_SUCCESS) {
        return false;
    }

    HKEY hBackup = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, backupPath.c_str(), 0, KEY_READ, &hBackup) == ERROR_SUCCESS) {
        WCHAR oldClsid[256] = {};
        DWORD cbData = sizeof(oldClsid);
        DWORD type = 0;
        if (RegQueryValueExW(hBackup, PKEY_FX_ENDPOINT, NULL, &type, (LPBYTE)oldClsid, &cbData) == ERROR_SUCCESS) {
            if (wcscmp(oldClsid, L"GUID_NA") == 0) {
                RegDeleteValueW(hFx, PKEY_FX_ENDPOINT);
                RegDeleteValueW(hFx, PKEY_COMPOSITE_FX_ENDPOINT);
            } else {
                RegSetValueExW(hFx, PKEY_FX_ENDPOINT, 0, type, (const BYTE*)oldClsid, cbData);
                std::wstring multiSz = std::wstring(oldClsid) + L'\0';
                RegSetValueExW(hFx, PKEY_COMPOSITE_FX_ENDPOINT, 0, REG_MULTI_SZ, (const BYTE*)multiSz.c_str(), (DWORD)((multiSz.length() + 1) * sizeof(WCHAR)));
            }
        }
        RegCloseKey(hBackup);
        RegDeleteTreeW(HKEY_LOCAL_MACHINE, backupPath.c_str());
    } else {
        // Fallback: delete if was set to our APO
        RegDeleteValueW(hFx, PKEY_FX_ENDPOINT);
        RegDeleteValueW(hFx, PKEY_COMPOSITE_FX_ENDPOINT);
    }

    RegCloseKey(hFx);
    return true;
}

bool ApoManager::InstallApoToAllEndpoints(const std::wstring& apoClsid) {
    auto endpoints = EnumerateEndpoints();
    bool allSuccess = true;
    for (const auto& ep : endpoints) {
        if (!InstallApoToEndpoint(ep.guid, apoClsid)) {
            allSuccess = false;
        }
    }
    return allSuccess;
}

bool ApoManager::UninstallApoFromAllEndpoints() {
    auto endpoints = EnumerateEndpoints();
    bool allSuccess = true;
    for (const auto& ep : endpoints) {
        if (!UninstallApoFromEndpoint(ep.guid)) {
            allSuccess = false;
        }
    }
    return allSuccess;
}

bool ApoManager::RegisterApoDll(const std::wstring& dllPath) {
    HKEY hKey = NULL;
    LPCWSTR clsidKey = L"SOFTWARE\\Classes\\CLSID\\{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}";
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, clsidKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }

    const WCHAR desc[] = L"Sbapo Global Effect";
    RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)desc, sizeof(desc));

    HKEY hInproc = NULL;
    if (RegCreateKeyExW(hKey, L"InprocServer32", 0, NULL, 0, KEY_WRITE, NULL, &hInproc, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hInproc, NULL, 0, REG_SZ, (const BYTE*)dllPath.c_str(), (DWORD)((dllPath.length() + 1) * sizeof(WCHAR)));
        const WCHAR model[] = L"Both";
        RegSetValueExW(hInproc, L"ThreadingModel", 0, REG_SZ, (const BYTE*)model, sizeof(model));
        RegCloseKey(hInproc);
    }

    RegCloseKey(hKey);
    return true;
}

bool ApoManager::UnregisterApoDll() {
    LPCWSTR clsidKey = L"SOFTWARE\\Classes\\CLSID\\{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}";
    return (RegDeleteTreeW(HKEY_LOCAL_MACHINE, clsidKey) == ERROR_SUCCESS);
}

void ApoManager::RestartAudioService() {
    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) return;

    SC_HANDLE hSvc = OpenServiceW(hSCM, L"AudioSrv", SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS);
    if (hSvc) {
        SERVICE_STATUS status;
        ControlService(hSvc, SERVICE_CONTROL_STOP, &status);
        Sleep(1000);
        StartServiceW(hSvc, 0, NULL);
        CloseServiceHandle(hSvc);
    }
    CloseServiceHandle(hSCM);
}
