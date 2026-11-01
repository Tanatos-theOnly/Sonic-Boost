#include <windows.h>
#include <commctrl.h>
#include <memory>
#include <string>
#include <cstdio>
#include "resource.h"
#include "SettingsManager.h"
#include "DspEngine.h"
#include "ApoController.h"
#include "ApoManager.h"
#include "AudioEngine.h"
#include "SliderPopup.h"
#include "TrayManager.h"
#include "SettingsDialog.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

#define HOTKEY_INC_ID 9001
#define HOTKEY_DEC_ID 9002

static const wchar_t* MAIN_WND_CLASS = L"SonicBoostMainWindowClass";
static const wchar_t* MUTEX_NAME = L"Local\\SonicBoost.InstanceMutex";

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

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* pExceptionInfo) {
    FILE* f = _wfopen(L"C:\\Users\\Public\\sonicboost.log", L"a, ccs=UTF-8");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fwprintf(f, L"[%02d:%02d:%02d.%03d] [CRASH] Exception Code: 0x%08X at Address: 0x%p\n",
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                 pExceptionInfo->ExceptionRecord->ExceptionCode,
                 pExceptionInfo->ExceptionRecord->ExceptionAddress);
        fflush(f);
        fclose(f);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}


class Application {
public:
    Application(HINSTANCE hInstance)
        : m_hInstance(hInstance),
          m_hWnd(NULL),
          m_settings(),
          m_dsp(),
          m_apo(),
          m_audio(m_dsp, m_apo)
    {
    }

    int Run() {
        LogDebug(L"Application starting...");

        // Ensure the main thread is attached to the user's interactive "Default" desktop
        HDESK hDefault = OpenDesktopW(L"Default", 0, FALSE,
            DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE |
            DESKTOP_HOOKCONTROL | DESKTOP_JOURNALPLAYBACK | DESKTOP_JOURNALRECORD |
            DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP | DESKTOP_WRITEOBJECTS);
        if (hDefault) {
            if (SetThreadDesktop(hDefault)) {
                LogDebug(L"Successfully attached to user's interactive Default desktop.");
            } else {
                LogDebug(L"SetThreadDesktop failed.");
            }
        }

        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

        // Ensure working directory is the application directory
        WCHAR szDir[MAX_PATH];
        GetModuleFileNameW(NULL, szDir, MAX_PATH);
        WCHAR* pSlash = wcsrchr(szDir, L'\\');
        if (pSlash) {
            *pSlash = L'\0';
            SetCurrentDirectoryW(szDir);
        }

        // Enforce single instance
        HANDLE hMutex = CreateMutexW(NULL, TRUE, MUTEX_NAME);
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            LogDebug(L"Another instance detected. Exiting.");
            HWND hWndExisting = FindWindowW(MAIN_WND_CLASS, NULL);
            if (hWndExisting) {
                PostMessageW(hWndExisting, WM_COMMAND, ID_BOOSTER_SHOW, 0);
            }
            if (hMutex) CloseHandle(hMutex);
            return 0;
        }

        // Initialize common controls
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
        InitCommonControlsEx(&icex);

        // Load settings
        m_settings.Load();

        // Register hidden controller window
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = Application::WndProc;
        wc.hInstance = m_hInstance;
        wc.lpszClassName = MAIN_WND_CLASS;
        RegisterClassExW(&wc);

        // Must be top-level (parent = NULL) so Shell_NotifyIconW can deliver tray messages
        m_hWnd = CreateWindowExW(
            0, MAIN_WND_CLASS, L"SonicBoost Controller",
            WS_OVERLAPPED, 0, 0, 0, 0,
            NULL, NULL, m_hInstance, this
        );

        if (!m_hWnd) {
            LogDebug(L"CreateWindowExW failed!");
            if (hMutex) CloseHandle(hMutex);
            return 1;
        }

        LogDebug(L"Controller window created successfully.");

        try {
            LogDebug(L"Creating SliderPopup...");
            m_popup = std::make_unique<SliderPopup>(m_hInstance, m_settings, m_audio);
            m_popup->Create();
            LogDebug(L"SliderPopup created.");

            LogDebug(L"Initializing TrayManager...");
            m_tray = std::make_unique<TrayManager>(m_hWnd, m_hInstance, m_settings, *m_popup);
            if (m_tray->Initialize()) {
                LogDebug(L"Tray icon initialized.");
            } else {
                LogDebug(L"Tray icon initialization failed.");
            }

            LogDebug(L"Initializing SettingsDialog...");
            m_settingsDlg = std::make_unique<SettingsDialog>(m_hInstance, m_hWnd, m_settings, *m_popup);

            // Register Global Hotkeys
            LogDebug(L"Registering hotkeys...");
            RegisterHotkeys();

            // Start Core Audio Engine and Hook Engine passing our top-level window handle
            LogDebug(L"Starting Audio Engine...");
            m_audio.SetBoostMethod(m_settings.GetBoostMethod());
            m_audio.Start(m_hWnd);
            m_audio.UpdateVolume(m_settings.GetSoundLevel());
            m_audio.SetEnabled(m_settings.IsBoostEnabled());
            m_audio.SetSuperBoost(m_settings.IsCompressorEnabled());
            LogDebug(L"Audio Engine started. Entering message loop.");

            // Display the volume slider popup immediately on startup on the user's desktop
            RECT rcWork = { 0 };
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
            POINT ptStartup;
            ptStartup.x = rcWork.right - 80;
            ptStartup.y = rcWork.bottom - 10;
            if (m_popup) {
                m_popup->Show(ptStartup);
            }
        } catch (const std::exception& e) {
            wchar_t szErr[256];
            swprintf_s(szErr, L"Exception in initialization: %S", e.what());
            LogDebug(szErr);
            return 1;
        } catch (...) {
            LogDebug(L"Unknown exception during initialization!");
            return 1;
        }

        // Message loop
        MSG msg;
        BOOL bRet;
        while ((bRet = GetMessageW(&msg, NULL, 0, 0)) != 0) {
            if (bRet == -1) {
                LogDebug(L"GetMessageW returned -1 (error)!");
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        wchar_t szExit[64];
        swprintf_s(szExit, L"Exiting message loop, wParam=%d", (int)msg.wParam);
        LogDebug(szExit);

        // Cleanup
        UnregisterHotkeys();
        m_audio.Stop();
        if (m_tray) m_tray->Remove();
        CloseHandle(hMutex);
        if (hDefault) CloseDesktop(hDefault);

        CoUninitialize();
        return (int)msg.wParam;
    }

private:
    void RegisterHotkeys() {
        UnregisterHotkeys();
        HotkeyConfig inc = m_settings.GetHotkeyInc();
        if (inc.enabled && inc.vkCode != 0) {
            RegisterHotKey(m_hWnd, HOTKEY_INC_ID, inc.modifiers, inc.vkCode);
        }
        HotkeyConfig dec = m_settings.GetHotkeyDec();
        if (dec.enabled && dec.vkCode != 0) {
            RegisterHotKey(m_hWnd, HOTKEY_DEC_ID, dec.modifiers, dec.vkCode);
        }
    }

    void UnregisterHotkeys() {
        UnregisterHotKey(m_hWnd, HOTKEY_INC_ID);
        UnregisterHotKey(m_hWnd, HOTKEY_DEC_ID);
    }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        Application* pApp = nullptr;
        if (msg == WM_NCCREATE) {
            CREATESTRUCT* pCS = (CREATESTRUCT*)lParam;
            pApp = (Application*)pCS->lpCreateParams;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)pApp);
        } else {
            pApp = (Application*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        }

        if (pApp) {
            return pApp->HandleMessage(hWnd, msg, wParam, lParam);
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_TRAYICON_NOTIFY:
            if (m_tray) m_tray->OnTrayNotification(lParam);
            return 0;

        case WM_HOTKEY: {
            int current = m_settings.GetSoundLevel();
            if (wParam == HOTKEY_INC_ID) {
                current += 10;
                if (current > 500) current = 500;
            } else if (wParam == HOTKEY_DEC_ID) {
                current -= 10;
                if (current < 100) current = 100;
            }
            m_settings.SetSoundLevel(current);
            m_settings.Save();
            if (m_popup) m_popup->UpdateSliderFromSettings();

            wchar_t szMsg[32];
            swprintf_s(szMsg, L"Volume: %d %%", current);
            if (m_tray) m_tray->ShowBalloon(L"SonicBoost", szMsg);
            return 0;
        }

        case WM_COMMAND: {
            int cmd = LOWORD(wParam);
            switch (cmd) {
            case ID_BOOSTER_SHOW: {
                POINT pt;
                GetCursorPos(&pt);
                RECT rcWork = { 0 };
                SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
                if (pt.x <= 0 && pt.y <= 0) {
                    pt.x = rcWork.right - 80;
                    pt.y = rcWork.bottom - 10;
                }
                if (m_popup) m_popup->Show(pt);
                return 0;
            }
            case ID_BOOSTER_SETTINGS:
                if (m_settingsDlg) m_settingsDlg->Show();
                return 0;

            case ID_BOOST_METHOD_HOOK:
                m_settings.SetBoostMethod(1);
                m_settings.Save();
                m_audio.SetBoostMethod(1);
                return 0;

            case ID_BOOST_METHOD_HOOK_APO:
                m_settings.SetBoostMethod(2);
                m_settings.Save();
                m_audio.SetBoostMethod(2);
                return 0;

            case ID_BOOST_METHOD_APO:
                m_settings.SetBoostMethod(3);
                m_settings.Save();
                m_audio.SetBoostMethod(3);
                return 0;

            case ID_USE_COMPRESSOR: {
                bool super = !m_settings.IsCompressorEnabled();
                m_settings.SetCompressorEnabled(super);
                m_settings.Save();
                m_audio.SetSuperBoost(super);
                return 0;
            }

            case ID_HELP_ABOUT:
                MessageBoxW(m_hWnd,
                    L"SonicBoost v1.13\n\n"
                    L"Developed by Reddy A. Rohan on earth.\n\n"
                    L"Architecture & Features:\n"
                    L"• Dual-Engine Audio Core (PCM In-Process Scaling up to 500% / +20 dB)\n"
                    L"• SuperBoost Peak Saturation & Dynamic Range Limiter\n"
                    L"• Real-Time Audio Endpoint & Device Switching\n"
                    L"• Low-Latency System-Wide Audio Amplification\n\n"
                    L"Status: 100% Free & Permanently Unlocked.",
                    L"About SonicBoost", MB_OK | MB_ICONINFORMATION);
                return 0;

            case ID_BOOSTER_EXIT:
                LogDebug(L"ID_BOOSTER_EXIT received.");
                PostQuitMessage(0);
                return 0;
            }
            break;
        }

        case WM_DESTROY:
            LogDebug(L"WM_DESTROY received.");
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    HINSTANCE m_hInstance;
    HWND m_hWnd;
    SettingsManager m_settings;
    DspEngine m_dsp;
    ApoController m_apo;
    AudioEngine m_audio;
    std::unique_ptr<SliderPopup> m_popup;
    std::unique_ptr<TrayManager> m_tray;
    std::unique_ptr<SettingsDialog> m_settingsDlg;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetUnhandledExceptionFilter(CrashHandler);
    if (strstr(lpCmdLine, "/install") || strstr(lpCmdLine, "-install")) {
        WCHAR szDir[MAX_PATH];
        GetModuleFileNameW(NULL, szDir, MAX_PATH);
        WCHAR* pSlash = wcsrchr(szDir, L'\\');
        if (pSlash) *pSlash = L'\0';
        std::wstring apoDllPath = std::wstring(szDir) + L"\\SoundBoosterApo.dll";
        ApoManager::RegisterApoDll(apoDllPath);
        ApoManager::InstallApoToAllEndpoints();
        ApoManager::RestartAudioService();
        MessageBoxW(NULL, L"SoundBooster APO installed successfully to all audio endpoints!", L"Installation Complete", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    if (strstr(lpCmdLine, "/uninstall") || strstr(lpCmdLine, "-uninstall")) {
        ApoManager::UninstallApoFromAllEndpoints();
        ApoManager::UnregisterApoDll();
        ApoManager::RestartAudioService();
        MessageBoxW(NULL, L"SoundBooster APO uninstalled successfully and endpoints restored.", L"Uninstallation Complete", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    Application app(hInstance);
    return app.Run();
}
