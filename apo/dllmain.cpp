#include <windows.h>
#include <olectl.h>
#include <strsafe.h>
#include <cstdio>
#include "SoundBoosterApo.h"

static void LogApo(const char* msg) {
    char buf[512];
    SYSTEMTIME st;
    GetLocalTime(&st);
    sprintf_s(buf, "[%02d:%02d:%02d.%03d] [PID %lu] [DLL] %s\n",
              st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentProcessId(), msg);
    OutputDebugStringA(buf);
    FILE* f = fopen("C:\\Users\\Public\\soundbooster_apo.log", "a");
    if (f) {
        fputs(buf, f);
        fflush(f);
        fclose(f);
    }
}

static HINSTANCE g_hModule = NULL;
static LONG g_serverLocks = 0;

class CClassFactory : public IClassFactory {
public:
    CClassFactory() : m_cRef(1) {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_cRef);
    }

    STDMETHODIMP_(ULONG) Release() override {
        ULONG ulRef = InterlockedDecrement(&m_cRef);
        if (ulRef == 0) delete this;
        return ulRef;
    }

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override {
        char initBuf[256];
        LPOLESTR pStr = NULL;
        StringFromIID(riid, &pStr);
        sprintf_s(initBuf, "CreateInstance called: pUnkOuter=%p, ppvObject=%p, riid=%ls",
                  pUnkOuter, ppvObject, pStr ? pStr : L"null");
        if (pStr) CoTaskMemFree(pStr);
        LogApo(initBuf);

        if (!ppvObject) {
            LogApo("CreateInstance returning E_POINTER (ppvObject is null)");
            return E_POINTER;
        }
        *ppvObject = nullptr;

        // In COM aggregation, if pUnkOuter is non-NULL, riid MUST be IID_IUnknown
        if (pUnkOuter != nullptr && riid != IID_IUnknown) {
            LogApo("Aggregation requested with non-IUnknown riid! Returning CLASS_E_NOAGGREGATION");
            return CLASS_E_NOAGGREGATION;
        }

        LogApo("About to create CSoundBoosterAPO...");
        CSoundBoosterAPO* pApo = nullptr;
        try {
            pApo = new CSoundBoosterAPO(pUnkOuter);
        } catch (...) {
            LogApo("EXCEPTION in new CSoundBoosterAPO()!");
            return E_FAIL;
        }

        if (!pApo) return E_OUTOFMEMORY;

        LogApo("Calling NonDelegatingQueryInterface on new APO...");
        HRESULT hr = E_FAIL;
        try {
            hr = pApo->NonDelegatingQueryInterface(riid, ppvObject);
        } catch (...) {
            LogApo("EXCEPTION in NonDelegatingQueryInterface!");
            pApo->NonDelegatingRelease();
            return E_FAIL;
        }

        char buf[128];
        sprintf_s(buf, "NonDelegatingQueryInterface returned 0x%08X, ppvObject = %p", hr, *ppvObject);
        LogApo(buf);

        pApo->NonDelegatingRelease();
        LogApo("CreateInstance done successfully");
        return hr;
    }

    STDMETHODIMP LockServer(BOOL fLock) override {
        if (fLock) {
            InterlockedIncrement(&g_serverLocks);
        } else {
            InterlockedDecrement(&g_serverLocks);
        }
        return S_OK;
    }

private:
    LONG m_cRef;
};

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hModule = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow() {
    return (g_serverLocks == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    LogApo("DllGetClassObject called");
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (rclsid == CLSID_SoundBoosterApo) {
        CClassFactory* pFactory = new CClassFactory();
        if (!pFactory) return E_OUTOFMEMORY;
        HRESULT hr = pFactory->QueryInterface(riid, ppv);
        pFactory->Release();
        return hr;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllRegisterServer() {
    LogApo("DllRegisterServer called");
    WCHAR szModule[MAX_PATH];
    if (GetModuleFileNameW(g_hModule, szModule, MAX_PATH) == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // 1. Register in HKLM\SOFTWARE\Classes\CLSID
    WCHAR szClsidKey[256];
    StringCchPrintfW(szClsidKey, 256, L"SOFTWARE\\Classes\\CLSID\\{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}");

    HKEY hKey = NULL;
    LONG lRes = RegCreateKeyExW(HKEY_LOCAL_MACHINE, szClsidKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (lRes == ERROR_SUCCESS) {
        const WCHAR szDesc[] = L"Sbapo Global Effect";
        RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)szDesc, sizeof(szDesc));

        HKEY hInproc = NULL;
        if (RegCreateKeyExW(hKey, L"InprocServer32", 0, NULL, 0, KEY_WRITE, NULL, &hInproc, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(hInproc, NULL, 0, REG_SZ, (const BYTE*)szModule, (DWORD)((wcslen(szModule) + 1) * sizeof(WCHAR)));
            const WCHAR szModel[] = L"Both";
            RegSetValueExW(hInproc, L"ThreadingModel", 0, REG_SZ, (const BYTE*)szModel, sizeof(szModel));
            RegCloseKey(hInproc);
        }
        RegCloseKey(hKey);
    }

    // 2. Also register in HKCU\Software\Classes\CLSID for user-space access
    WCHAR szUserClsidKey[256];
    StringCchPrintfW(szUserClsidKey, 256, L"Software\\Classes\\CLSID\\{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}");
    HKEY hUserKey = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, szUserClsidKey, 0, NULL, 0, KEY_WRITE, NULL, &hUserKey, NULL) == ERROR_SUCCESS) {
        const WCHAR szDesc[] = L"Sbapo Global Effect";
        RegSetValueExW(hUserKey, NULL, 0, REG_SZ, (const BYTE*)szDesc, sizeof(szDesc));

        HKEY hInproc = NULL;
        if (RegCreateKeyExW(hUserKey, L"InprocServer32", 0, NULL, 0, KEY_WRITE, NULL, &hInproc, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(hInproc, NULL, 0, REG_SZ, (const BYTE*)szModule, (DWORD)((wcslen(szModule) + 1) * sizeof(WCHAR)));
            const WCHAR szModel[] = L"Both";
            RegSetValueExW(hInproc, L"ThreadingModel", 0, REG_SZ, (const BYTE*)szModel, sizeof(szModel));
            RegCloseKey(hInproc);
        }
        RegCloseKey(hUserKey);
    }

    // 3. Register with standard Windows Audio Engine RegisterAPO API
    const APO_REG_PROPERTIES* pProps = GetApoRegProperties();
    HRESULT hrApo = RegisterAPO(pProps);
    char apoLog[128];
    sprintf_s(apoLog, "RegisterAPO result: 0x%08X", hrApo);
    LogApo(apoLog);

    // 4. Directly ensure HKLM\SOFTWARE\Classes\AudioEngine\AudioProcessingObjects has all values
    WCHAR szApoKey[256];
    StringCchPrintfW(szApoKey, 256, L"SOFTWARE\\Classes\\AudioEngine\\AudioProcessingObjects\\{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}");
    HKEY hApo = NULL;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, szApoKey, 0, NULL, 0, KEY_WRITE, NULL, &hApo, NULL) == ERROR_SUCCESS) {
        const WCHAR szFriendly[] = L"SonicBoost Global Audio Effect";
        RegSetValueExW(hApo, L"FriendlyName", 0, REG_SZ, (const BYTE*)szFriendly, sizeof(szFriendly));

        const WCHAR szCopyright[] = L"Developed by Reddy A. Rohan on earth";
        RegSetValueExW(hApo, L"Copyright", 0, REG_SZ, (const BYTE*)szCopyright, sizeof(szCopyright));

        DWORD dwMajor = 1;
        RegSetValueExW(hApo, L"MajorVersion", 0, REG_DWORD, (const BYTE*)&dwMajor, sizeof(DWORD));

        DWORD dwMinor = 0;
        RegSetValueExW(hApo, L"MinorVersion", 0, REG_DWORD, (const BYTE*)&dwMinor, sizeof(DWORD));

        DWORD dwFlags = 0x0000000d;
        RegSetValueExW(hApo, L"Flags", 0, REG_DWORD, (const BYTE*)&dwFlags, sizeof(DWORD));

        DWORD dwOne = 1;
        RegSetValueExW(hApo, L"MinInputConnections", 0, REG_DWORD, (const BYTE*)&dwOne, sizeof(DWORD));
        RegSetValueExW(hApo, L"MaxInputConnections", 0, REG_DWORD, (const BYTE*)&dwOne, sizeof(DWORD));
        RegSetValueExW(hApo, L"MinOutputConnections", 0, REG_DWORD, (const BYTE*)&dwOne, sizeof(DWORD));
        RegSetValueExW(hApo, L"MaxOutputConnections", 0, REG_DWORD, (const BYTE*)&dwOne, sizeof(DWORD));

        DWORD dwMaxInst = 0xFFFFFFFF;
        RegSetValueExW(hApo, L"MaxInstances", 0, REG_DWORD, (const BYTE*)&dwMaxInst, sizeof(DWORD));

        DWORD dwNumIfs = 1;
        RegSetValueExW(hApo, L"NumAPOInterfaces", 0, REG_DWORD, (const BYTE*)&dwNumIfs, sizeof(DWORD));

        const WCHAR szIf0[] = L"{FD7F2B29-24D0-4B5C-B177-592C39F9CA10}";
        RegSetValueExW(hApo, L"APOInterface0", 0, REG_SZ, (const BYTE*)szIf0, sizeof(szIf0));

        RegCloseKey(hApo);
    }

    LogApo("DllRegisterServer finished successfully");
    return S_OK;
}

STDAPI DllUnregisterServer() {
    LogApo("DllUnregisterServer called");
    UnregisterAPO(CLSID_SoundBoosterApo);

    WCHAR szKey[256];
    StringCchPrintfW(szKey, 256, L"SOFTWARE\\Classes\\CLSID\\{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}");
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, szKey);

    StringCchPrintfW(szKey, 256, L"Software\\Classes\\CLSID\\{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}");
    RegDeleteTreeW(HKEY_CURRENT_USER, szKey);

    return S_OK;
}
