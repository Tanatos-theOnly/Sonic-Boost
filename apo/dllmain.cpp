#include <windows.h>
#include <olectl.h>
#include <strsafe.h>
#include "SoundBoosterApo.h"

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
        if (!ppvObject) return E_POINTER;
        *ppvObject = nullptr;
        if (pUnkOuter != nullptr) return CLASS_E_NOAGGREGATION;

        CSoundBoosterAPO* pApo = new CSoundBoosterAPO();
        if (!pApo) return E_OUTOFMEMORY;

        HRESULT hr = pApo->QueryInterface(riid, ppvObject);
        pApo->Release();
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
    WCHAR szModule[MAX_PATH];
    if (GetModuleFileNameW(g_hModule, szModule, MAX_PATH) == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    WCHAR szKey[256];
    StringCchPrintfW(szKey, 256, L"SOFTWARE\\Classes\\CLSID\\{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}");

    HKEY hKey = NULL;
    LONG lRes = RegCreateKeyExW(HKEY_LOCAL_MACHINE, szKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (lRes != ERROR_SUCCESS) return HRESULT_FROM_WIN32(lRes);

    const WCHAR szDesc[] = L"Sbapo Global Effect";
    RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)szDesc, sizeof(szDesc));

    HKEY hInproc = NULL;
    lRes = RegCreateKeyExW(hKey, L"InprocServer32", 0, NULL, 0, KEY_WRITE, NULL, &hInproc, NULL);
    if (lRes == ERROR_SUCCESS) {
        RegSetValueExW(hInproc, NULL, 0, REG_SZ, (const BYTE*)szModule, (DWORD)((wcslen(szModule) + 1) * sizeof(WCHAR)));
        const WCHAR szModel[] = L"Both";
        RegSetValueExW(hInproc, L"ThreadingModel", 0, REG_SZ, (const BYTE*)szModel, sizeof(szModel));
        RegCloseKey(hInproc);
    }
    RegCloseKey(hKey);

    return S_OK;
}

STDAPI DllUnregisterServer() {
    WCHAR szKey[256];
    StringCchPrintfW(szKey, 256, L"SOFTWARE\\Classes\\CLSID\\{BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}");
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, szKey);
    return S_OK;
}
