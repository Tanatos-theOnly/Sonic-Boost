#include "ApoController.h"
#include <cstdio>
#include <iostream>

ApoController::ApoController()
    : m_hMapFile(NULL),
      m_pSharedBuf(nullptr),
      m_hUpdateEvent(NULL),
      m_hCreateEvent(NULL),
      m_volumePercent(100),
      m_enabled(true),
      m_superBoost(true)
{
    InitializeCriticalSection(&m_cs);
}

ApoController::~ApoController() {
    Shutdown();
    DeleteCriticalSection(&m_cs);
}

bool ApoController::EnablePrivilege(LPCWSTR privilegeName) {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    LUID luid;
    if (!LookupPrivilegeValueW(NULL, privilegeName, &luid)) {
        CloseHandle(hToken);
        return false;
    }

    TOKEN_PRIVILEGES tp = {};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
    DWORD err = GetLastError();
    CloseHandle(hToken);

    return (err == ERROR_SUCCESS);
}

bool ApoController::SetupSecurityAttributes(SECURITY_ATTRIBUTES& sa, PSECURITY_DESCRIPTOR& pSD) {
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = FALSE;
    pSD = nullptr;

    // SDDL allowing Authenticated Users, Administrators, World, and System with Low Integrity level
    LPCWSTR sddl = L"D:(A;OICI;GA;;;AU)(A;OICI;GA;;;BA)(A;OICI;GA;;;WD)(A;OICI;GA;;;SY)S:(ML;;NWNR;;;LW)";
    if (ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &pSD, NULL)) {
        sa.lpSecurityDescriptor = pSD;
        return true;
    }
    sa.lpSecurityDescriptor = NULL;
    return false;
}

bool ApoController::Initialize() {
    EnterCriticalSection(&m_cs);

    // Attempt to acquire SeCreateGlobalPrivilege
    EnablePrivilege(SE_CREATE_GLOBAL_NAME);

    SECURITY_ATTRIBUTES sa = {};
    PSECURITY_DESCRIPTOR pSD = nullptr;
    SetupSecurityAttributes(sa, pSD);

    // 1. Open or create the shared configuration memory
    m_hMapFile = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, L"Global\\SbapoConfigData");
    bool newlyCreated = false;

    if (!m_hMapFile) {
        m_hMapFile = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            &sa,
            PAGE_READWRITE,
            0,
            256,
            L"Global\\SbapoConfigData"
        );
        if (m_hMapFile) {
            newlyCreated = true;
        }
    }

    if (m_hMapFile) {
        m_pSharedBuf = MapViewOfFile(m_hMapFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 256);
    }

    // 2. Open or create update and create events
    m_hUpdateEvent = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, L"Global\\SbapoDataUpdate");
    if (!m_hUpdateEvent) {
        m_hUpdateEvent = CreateEventW(&sa, FALSE, FALSE, L"Global\\SbapoDataUpdate");
    }

    m_hCreateEvent = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, L"Global\\SbapoDataCreate");
    if (!m_hCreateEvent) {
        m_hCreateEvent = CreateEventW(&sa, FALSE, FALSE, L"Global\\SbapoDataCreate");
    }

    if (newlyCreated && m_hCreateEvent) {
        SetEvent(m_hCreateEvent);
    }

    if (pSD) {
        LocalFree(pSD);
    }

    // Apply current initial configuration
    UpdateSharedData();

    LeaveCriticalSection(&m_cs);
    return (m_pSharedBuf != nullptr);
}

void ApoController::Shutdown() {
    EnterCriticalSection(&m_cs);

    if (m_pSharedBuf) {
        // Reset to unity before closing
        sprintf_s((char*)m_pSharedBuf, 256, "# Boost: 0 dB 0\nEnableOldApo: 0\n");
        if (m_hUpdateEvent) {
            SetEvent(m_hUpdateEvent);
        }
        UnmapViewOfFile(m_pSharedBuf);
        m_pSharedBuf = nullptr;
    }

    if (m_hMapFile) {
        CloseHandle(m_hMapFile);
        m_hMapFile = NULL;
    }
    if (m_hUpdateEvent) {
        CloseHandle(m_hUpdateEvent);
        m_hUpdateEvent = NULL;
    }
    if (m_hCreateEvent) {
        CloseHandle(m_hCreateEvent);
        m_hCreateEvent = NULL;
    }

    LeaveCriticalSection(&m_cs);
}

void ApoController::SetVolume(int volumePercent) {
    if (volumePercent < 100) volumePercent = 100;
    if (volumePercent > 500) volumePercent = 500;
    m_volumePercent = volumePercent;
    UpdateSharedData();
}

void ApoController::SetEnabled(bool enabled) {
    m_enabled = enabled;
    UpdateSharedData();
}

void ApoController::SetSuperBoost(bool superBoost) {
    m_superBoost = superBoost;
    UpdateSharedData();
}

void ApoController::UpdateSharedData() {
    EnterCriticalSection(&m_cs);

    if (m_pSharedBuf) {
        int vol = m_volumePercent.load();
        bool en = m_enabled.load();

        if (!en || vol <= 100) {
            // Disabled or Unity gain
            sprintf_s((char*)m_pSharedBuf, 256, "# Boost: 0 dB 0\nEnableOldApo: 0\n");
        } else {
            // Exact formula: MulDiv(vol - 100, 1, 2)
            int boostDb = MulDiv(vol - 100, 1, 2);
            sprintf_s((char*)m_pSharedBuf, 256, "Boost: %d dB 1\nEnableOldApo: 0\n", boostDb);
        }

        if (m_hUpdateEvent) {
            SetEvent(m_hUpdateEvent);
        }
    }

    LeaveCriticalSection(&m_cs);
}
