#include "ApoController.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <sddl.h>

static void LogDebug(const wchar_t* msg) {
    FILE* f = _wfopen(L"C:\\Users\\Public\\sonicboost.log", L"a, ccs=UTF-8");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fwprintf(f, L"[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
        fflush(f);
        fclose(f);
    }
}

ApoController::ApoController()
    : m_hMapFile(NULL),
      m_pSharedBuf(nullptr),
      m_hUpdateEvent(NULL),
      m_hCreateEvent(NULL),
      m_hStopEvent(NULL),
      m_hWatcherThread(NULL),
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

bool ApoController::TryConnect() {
    if (m_hMapFile && m_pSharedBuf) {
        return true;
    }

    if (!m_hMapFile) {
        // Try to open existing mapping first (created by APO in audiodg)
        m_hMapFile = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, L"Global\\SbapoConfigData");

        if (!m_hMapFile) {
            // Pre-create the shared memory with Low-IL SDDL so both audiodg and user app can access it
            PSECURITY_DESCRIPTOR pSd = NULL;
            if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
                L"D:(A;OICI;GA;;;AU)(A;OICI;GA;;;BA)(A;OICI;GA;;;WD)(A;OICI;GA;;;SY)S:(ML;;NWNR;;;LW)",
                SDDL_REVISION_1, &pSd, NULL))
            {
                SECURITY_ATTRIBUTES sa = { sizeof(sa), pSd, FALSE };
                m_hMapFile = CreateFileMappingW(INVALID_HANDLE_VALUE, &sa,
                                                PAGE_READWRITE, 0, 256, L"Global\\SbapoConfigData");
                if (m_hMapFile) {
                    LogDebug(L"[APO] Created Global\\SbapoConfigData shared memory (pre-created by app).");
                    void* pInit = MapViewOfFile(m_hMapFile, FILE_MAP_WRITE, 0, 0, 256);
                    if (pInit) {
                        sprintf_s((char*)pInit, 256, "# Boost: 0 dB 0\nEnableOldApo: 0\n");
                        UnmapViewOfFile(pInit);
                    }
                }
                LocalFree(pSd);
            }
        }
    }

    if (m_hMapFile && !m_pSharedBuf) {
        m_pSharedBuf = MapViewOfFile(m_hMapFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 256);
    }

    if (!m_hUpdateEvent) {
        m_hUpdateEvent = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, L"Global\\SbapoDataUpdate");
        if (!m_hUpdateEvent) {
            PSECURITY_DESCRIPTOR pSd = NULL;
            if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
                L"D:(A;OICI;GA;;;AU)(A;OICI;GA;;;BA)(A;OICI;GA;;;WD)(A;OICI;GA;;;SY)S:(ML;;NWNR;;;LW)",
                SDDL_REVISION_1, &pSd, NULL))
            {
                SECURITY_ATTRIBUTES sa = { sizeof(sa), pSd, FALSE };
                m_hUpdateEvent = CreateEventW(&sa, FALSE, FALSE, L"Global\\SbapoDataUpdate");
                LocalFree(pSd);
            }
        }
    }

    if (!m_hCreateEvent) {
        m_hCreateEvent = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"Global\\SbapoDataCreate");
        if (!m_hCreateEvent) {
            PSECURITY_DESCRIPTOR pSd = NULL;
            if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
                L"D:(A;OICI;GA;;;AU)(A;OICI;GA;;;BA)(A;OICI;GA;;;WD)(A;OICI;GA;;;SY)S:(ML;;NWNR;;;LW)",
                SDDL_REVISION_1, &pSd, NULL))
            {
                SECURITY_ATTRIBUTES sa = { sizeof(sa), pSd, FALSE };
                m_hCreateEvent = CreateEventW(&sa, FALSE, FALSE, L"Global\\SbapoDataCreate");
                LocalFree(pSd);
            }
        }
    }

    return (m_pSharedBuf != nullptr);
}

bool ApoController::Initialize() {
    EnterCriticalSection(&m_cs);

    // Initial connection attempt (will succeed if audio is already playing)
    TryConnect();

    // Create shutdown event for watcher thread
    m_hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

    // Start background watcher thread to dynamically connect/reconnect whenever audiodg.exe runs
    m_hWatcherThread = CreateThread(NULL, 0, WatcherThreadProc, this, 0, NULL);

    if (m_pSharedBuf) {
        LogDebug(L"[APO] Connected to system APO shared memory.");
    } else {
        LogDebug(L"[APO] Background watcher thread active (listening for audio events).");
    }

    // Apply current initial settings
    UpdateSharedData();

    LeaveCriticalSection(&m_cs);
    return true;
}

DWORD WINAPI ApoController::WatcherThreadProc(LPVOID lpParam) {
    ApoController* pThis = (ApoController*)lpParam;
    if (pThis) {
        pThis->WatcherLoop();
    }
    return 0;
}

void ApoController::WatcherLoop() {
    while (true) {
        // If create event is not yet opened, attempt to open it
        if (!m_hCreateEvent) {
            m_hCreateEvent = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"Global\\SbapoDataCreate");
        }

        HANDLE handles[2];
        DWORD count = 0;
        handles[count++] = m_hStopEvent;
        if (m_hCreateEvent) {
            handles[count++] = m_hCreateEvent;
        }

        DWORD dwWait = WaitForMultipleObjects(count, handles, FALSE, 500);
        if (dwWait == WAIT_OBJECT_0) {
            // Stop event signaled: exit thread cleanly
            break;
        } else if (dwWait == WAIT_OBJECT_0 + 1) {
            // Global\SbapoDataCreate signaled: audiodg.exe created/re-created the memory
            if (m_hCreateEvent) {
                ResetEvent(m_hCreateEvent);
            }
            EnterCriticalSection(&m_cs);
            if (m_pSharedBuf) {
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
            if (TryConnect()) {
                LogDebug(L"[APO] Reconnected to system APO shared memory via Create event.");
                UpdateSharedData();
            }
            LeaveCriticalSection(&m_cs);
            Sleep(50);
        } else if (dwWait == WAIT_TIMEOUT) {
            // Periodic keep-alive check: if not connected, attempt auto-reconnect
            EnterCriticalSection(&m_cs);
            if (!m_pSharedBuf) {
                if (TryConnect()) {
                    LogDebug(L"[APO] Reconnected to system APO shared memory.");
                    UpdateSharedData();
                }
            } else {
                // Keep shared data actively synced
                UpdateSharedData();
            }
            LeaveCriticalSection(&m_cs);
        }
    }
}

void ApoController::Shutdown() {
    EnterCriticalSection(&m_cs);

    // Stop watcher thread
    if (m_hStopEvent) {
        SetEvent(m_hStopEvent);
    }
    if (m_hWatcherThread) {
        WaitForSingleObject(m_hWatcherThread, 1500);
        CloseHandle(m_hWatcherThread);
        m_hWatcherThread = NULL;
    }
    if (m_hStopEvent) {
        CloseHandle(m_hStopEvent);
        m_hStopEvent = NULL;
    }

    if (m_pSharedBuf) {
        // Reset APO to unity gain (0 dB) before exiting
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

    LogDebug(L"[APO] ApoController shutdown cleanly.");

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

    // If not currently connected, attempt on-demand connection
    if (!m_pSharedBuf) {
        TryConnect();
    }

    int vol = m_volumePercent.load();
    bool en = m_enabled.load();
    bool super = m_superBoost.load();

    char cfgStr[256] = {};
    if (!en || vol <= 100) {
        // Unity gain / bypass
        sprintf_s(cfgStr, sizeof(cfgStr), "# Boost: 0 dB 0\nEnableOldApo: 0\n");
    } else {
        // Convert volume percentage (100% to 500%) to dB:
        // 100% -> 0 dB, 200% -> +6 dB, 300% -> +10 dB, 400% -> +12 dB, 500% -> +14 dB
        int boostDb = (int)(20.0 * log10((double)vol / 100.0) + 0.5);
        if (boostDb < 1) boostDb = 1;
        sprintf_s(cfgStr, sizeof(cfgStr), "Boost: %d dB %d\nEnableOldApo: 0\n", boostDb, super ? 1 : 0);
    }

    // Always persist state to file so cold boots and audiodg.exe restarts pick it up immediately
    FILE* fState = fopen("C:\\Users\\Public\\sbapo_state.txt", "w");
    if (fState) {
        fputs(cfgStr, fState);
        fflush(fState);
        fclose(fState);
    }

    if (m_pSharedBuf) {
        sprintf_s((char*)m_pSharedBuf, 256, "%s", cfgStr);
        if (m_hUpdateEvent) {
            SetEvent(m_hUpdateEvent);
        }
    }

    LeaveCriticalSection(&m_cs);
}
