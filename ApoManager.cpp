#include "ApoManager.h"
#include <iostream>
#include <vector>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

static const WCHAR RENDER_BASE_KEY[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Render";
static const WCHAR BACKUP_BASE_KEY[] = L"SOFTWARE\\SonicBoost\\OriginalEndpointEffects";

static const WCHAR PKEY_FX_POSTMIX[] = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},2";
static const WCHAR PKEY_FX_ENDPOINT_EFFECT[] = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},7";
static const WCHAR PKEY_COMPOSITE_FX_ENDPOINT[] = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},15";
static const WCHAR PKEY_COMPOSITE_FX_MODE[] = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},14";
static const WCHAR PKEY_COMPOSITE_FX_OFFLOAD_MODE[] = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},20";

static std::vector<std::wstring> ReadMultiSz(HKEY hKey, LPCWSTR valueName) {
    std::vector<std::wstring> result;
    DWORD cbData = 0;
    DWORD type = 0;
    if (RegQueryValueExW(hKey, valueName, NULL, &type, NULL, &cbData) != ERROR_SUCCESS || cbData == 0) {
        return result;
    }
    if (type != REG_MULTI_SZ && type != REG_SZ) {
        return result;
    }

    std::vector<WCHAR> buf(cbData / sizeof(WCHAR) + 2, 0);
    if (RegQueryValueExW(hKey, valueName, NULL, &type, (LPBYTE)buf.data(), &cbData) == ERROR_SUCCESS) {
        if (type == REG_MULTI_SZ) {
            LPCWSTR p = buf.data();
            while (*p) {
                result.push_back(p);
                p += wcslen(p) + 1;
            }
        } else if (type == REG_SZ) {
            result.push_back(buf.data());
        }
    }
    return result;
}

static bool WriteMultiSz(HKEY hKey, LPCWSTR valueName, const std::vector<std::wstring>& list) {
    std::vector<WCHAR> multiSz;
    for (const auto& s : list) {
        multiSz.insert(multiSz.end(), s.begin(), s.end());
        multiSz.push_back(L'\0');
    }
    multiSz.push_back(L'\0'); // Double null terminator
    DWORD cb = (DWORD)(multiSz.size() * sizeof(WCHAR));
    return RegSetValueExW(hKey, valueName, 0, REG_MULTI_SZ, (const BYTE*)multiSz.data(), cb) == ERROR_SUCCESS;
}

static bool ContainsClsid(const std::vector<std::wstring>& list, const std::wstring& clsid) {
    for (const auto& item : list) {
        if (_wcsicmp(item.c_str(), clsid.c_str()) == 0) return true;
    }
    return false;
}

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
            const std::wstring target = L"{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}";

            WCHAR clsidVal[256] = {};
            DWORD cbData = sizeof(clsidVal);
            if (RegQueryValueExW(hFx, PKEY_FX_POSTMIX, NULL, NULL, (LPBYTE)clsidVal, &cbData) == ERROR_SUCCESS) {
                if (_wcsicmp(clsidVal, target.c_str()) == 0) {
                    info.isApoInstalled = true;
                }
            }

            if (!info.isApoInstalled) {
                auto list15 = ReadMultiSz(hFx, PKEY_COMPOSITE_FX_ENDPOINT);
                if (ContainsClsid(list15, target)) info.isApoInstalled = true;
            }
            if (!info.isApoInstalled) {
                auto list20 = ReadMultiSz(hFx, PKEY_COMPOSITE_FX_OFFLOAD_MODE);
                if (ContainsClsid(list20, target)) info.isApoInstalled = true;
            }
            if (!info.isApoInstalled) {
                auto list14 = ReadMultiSz(hFx, PKEY_COMPOSITE_FX_MODE);
                if (ContainsClsid(list14, target)) info.isApoInstalled = true;
            }

            RegCloseKey(hFx);
        }

        endpoints.push_back(info);
    }

    RegCloseKey(hBase);
    return endpoints;
}

static void BackupProperty(HKEY hFx, HKEY hBackup, LPCWSTR valueName) {
    if (!hBackup) return;
    DWORD type = 0;
    DWORD cbData = 0;
    if (RegQueryValueExW(hFx, valueName, NULL, &type, NULL, &cbData) == ERROR_SUCCESS && cbData > 0) {
        std::vector<BYTE> data(cbData);
        if (RegQueryValueExW(hFx, valueName, NULL, &type, data.data(), &cbData) == ERROR_SUCCESS) {
            RegSetValueExW(hBackup, valueName, 0, type, data.data(), cbData);
        }
    } else {
        const WCHAR na[] = L"GUID_NA";
        RegSetValueExW(hBackup, valueName, 0, REG_SZ, (const BYTE*)na, sizeof(na));
    }
}

static void RestoreProperty(HKEY hFx, HKEY hBackup, LPCWSTR valueName) {
    if (!hBackup) return;
    DWORD type = 0;
    DWORD cbData = 0;
    if (RegQueryValueExW(hBackup, valueName, NULL, &type, NULL, &cbData) == ERROR_SUCCESS && cbData > 0) {
        std::vector<BYTE> data(cbData);
        if (RegQueryValueExW(hBackup, valueName, NULL, &type, data.data(), &cbData) == ERROR_SUCCESS) {
            if (type == REG_SZ && wcscmp((LPCWSTR)data.data(), L"GUID_NA") == 0) {
                RegDeleteValueW(hFx, valueName);
            } else {
                RegSetValueExW(hFx, valueName, 0, type, data.data(), cbData);
            }
        }
    }
}

bool ApoManager::InstallApoToEndpoint(const std::wstring& endpointGuid, const std::wstring& apoClsid) {
    std::wstring fxPath = std::wstring(RENDER_BASE_KEY) + L"\\" + endpointGuid + L"\\FxProperties";
    std::wstring backupPath = std::wstring(BACKUP_BASE_KEY) + L"\\" + endpointGuid;

    HKEY hBackup = NULL;
    RegCreateKeyExW(HKEY_LOCAL_MACHINE, backupPath.c_str(), 0, NULL, 0, KEY_WRITE | KEY_READ, NULL, &hBackup, NULL);

    HKEY hFx = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, fxPath.c_str(), 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hFx) != ERROR_SUCCESS) {
        if (hBackup) RegCloseKey(hBackup);
        return false;
    }

    // Backup original values before making any modifications
    if (hBackup) {
        BackupProperty(hFx, hBackup, PKEY_FX_POSTMIX);
        BackupProperty(hFx, hBackup, PKEY_FX_ENDPOINT_EFFECT);
        BackupProperty(hFx, hBackup, PKEY_COMPOSITE_FX_ENDPOINT);
        BackupProperty(hFx, hBackup, PKEY_COMPOSITE_FX_OFFLOAD_MODE);
        BackupProperty(hFx, hBackup, PKEY_COMPOSITE_FX_MODE);
        RegCloseKey(hBackup);
    }

    // 1. Install to PKEY_FX_POSTMIX (Legacy GFX key 2)
    DWORD cbData = (DWORD)((apoClsid.length() + 1) * sizeof(WCHAR));
    RegSetValueExW(hFx, PKEY_FX_POSTMIX, 0, REG_SZ, (const BYTE*)apoClsid.c_str(), cbData);

    // 2. Install to PKEY_COMPOSITE_FX_ENDPOINT (CompositeFX key 15)
    auto list15 = ReadMultiSz(hFx, PKEY_COMPOSITE_FX_ENDPOINT);
    if (!ContainsClsid(list15, apoClsid)) {
        list15.push_back(apoClsid);
    }
    WriteMultiSz(hFx, PKEY_COMPOSITE_FX_ENDPOINT, list15);

    // 3. If Modern Windows 10/11 CompositeFX Mode Effect (key 20) is present on endpoint, append our APO
    DWORD cb20 = 0;
    if (RegQueryValueExW(hFx, PKEY_COMPOSITE_FX_OFFLOAD_MODE, NULL, NULL, NULL, &cb20) == ERROR_SUCCESS && cb20 > 0) {
        auto list20 = ReadMultiSz(hFx, PKEY_COMPOSITE_FX_OFFLOAD_MODE);
        if (!ContainsClsid(list20, apoClsid)) {
            list20.push_back(apoClsid);
            WriteMultiSz(hFx, PKEY_COMPOSITE_FX_OFFLOAD_MODE, list20);
        }
    }

    // 4. If CompositeFX Mode Effect (key 14) is present, append our APO
    DWORD cb14 = 0;
    if (RegQueryValueExW(hFx, PKEY_COMPOSITE_FX_MODE, NULL, NULL, NULL, &cb14) == ERROR_SUCCESS && cb14 > 0) {
        auto list14 = ReadMultiSz(hFx, PKEY_COMPOSITE_FX_MODE);
        if (!ContainsClsid(list14, apoClsid)) {
            list14.push_back(apoClsid);
            WriteMultiSz(hFx, PKEY_COMPOSITE_FX_MODE, list14);
        }
    }

    // 5. Ensure System Audio Effects & enhancements are enabled
    DWORD dwZero = 0;
    RegSetValueExW(hFx, L"{E9914457-331A-41B5-88A9-0E5930115576},0", 0, REG_DWORD, (const BYTE*)&dwZero, sizeof(DWORD));
    RegSetValueExW(hFx, L"{1da5d803-d492-4edd-8c23-e0c0ffee7f0e},5", 0, REG_DWORD, (const BYTE*)&dwZero, sizeof(DWORD));

    RegCloseKey(hFx);
    return true;
}

bool ApoManager::IsApoInstalledOnAnyEndpoint() {
    auto endpoints = EnumerateEndpoints();
    for (const auto& ep : endpoints) {
        if (ep.isApoInstalled) return true;
    }
    return false;
}

bool ApoManager::UninstallApoFromEndpoint(const std::wstring& endpointGuid) {
    std::wstring fxPath = std::wstring(RENDER_BASE_KEY) + L"\\" + endpointGuid + L"\\FxProperties";
    std::wstring backupPath = std::wstring(BACKUP_BASE_KEY) + L"\\" + endpointGuid;

    HKEY hFx = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, fxPath.c_str(), 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hFx) != ERROR_SUCCESS) {
        return false;
    }

    HKEY hBackup = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, backupPath.c_str(), 0, KEY_READ, &hBackup) == ERROR_SUCCESS) {
        RestoreProperty(hFx, hBackup, PKEY_FX_POSTMIX);
        RestoreProperty(hFx, hBackup, PKEY_FX_ENDPOINT_EFFECT);
        RestoreProperty(hFx, hBackup, PKEY_COMPOSITE_FX_ENDPOINT);
        RestoreProperty(hFx, hBackup, PKEY_COMPOSITE_FX_OFFLOAD_MODE);
        RestoreProperty(hFx, hBackup, PKEY_COMPOSITE_FX_MODE);

        RegCloseKey(hBackup);
        RegDeleteTreeW(HKEY_LOCAL_MACHINE, backupPath.c_str());
    } else {
        // Fallback: clean removal of our APO CLSID
        const std::wstring target = L"{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}";

        auto CleanMulti = [&](LPCWSTR valName) {
            auto list = ReadMultiSz(hFx, valName);
            std::vector<std::wstring> cleaned;
            for (const auto& item : list) {
                if (_wcsicmp(item.c_str(), target.c_str()) != 0) cleaned.push_back(item);
            }
            if (cleaned.empty()) {
                RegDeleteValueW(hFx, valName);
            } else {
                WriteMultiSz(hFx, valName, cleaned);
            }
        };

        CleanMulti(PKEY_COMPOSITE_FX_ENDPOINT);
        CleanMulti(PKEY_COMPOSITE_FX_OFFLOAD_MODE);
        CleanMulti(PKEY_COMPOSITE_FX_MODE);

        WCHAR clsidVal[256] = {};
        DWORD cbData = sizeof(clsidVal);
        if (RegQueryValueExW(hFx, PKEY_FX_POSTMIX, NULL, NULL, (LPBYTE)clsidVal, &cbData) == ERROR_SUCCESS) {
            if (_wcsicmp(clsidVal, target.c_str()) == 0) {
                RegDeleteValueW(hFx, PKEY_FX_POSTMIX);
            }
        }
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
    // 1. Invoke self-registration inside the APO DLL (RegisterAPO, AudioProcessingObjects keys)
    typedef HRESULT (STDAPICALLTYPE *PFN_DllRegisterServer)();
    HMODULE hDll = LoadLibraryW(dllPath.c_str());
    if (hDll) {
        PFN_DllRegisterServer pfnReg = (PFN_DllRegisterServer)GetProcAddress(hDll, "DllRegisterServer");
        if (pfnReg) {
            pfnReg();
        }
        FreeLibrary(hDll);
    }

    // 2. Ensure HKLM\SOFTWARE\Classes\CLSID has InprocServer32 explicitly pointing to dllPath
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
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, clsidKey);
    LPCWSTR apoKey = L"SOFTWARE\\Classes\\AudioEngine\\AudioProcessingObjects\\{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}";
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, apoKey);
    return true;
}

void ApoManager::RestartAudioService() {
    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCM) {
        SC_HANDLE hSvc = OpenServiceW(hSCM, L"AudioSrv", SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS);
        if (hSvc) {
            SERVICE_STATUS status;
            ControlService(hSvc, SERVICE_CONTROL_STOP, &status);
            Sleep(1500);
            StartServiceW(hSvc, 0, NULL);
            CloseServiceHandle(hSvc);
            CloseServiceHandle(hSCM);
            return;
        }
        CloseServiceHandle(hSCM);
    }
}
