#pragma once
#include <windows.h>
#include <shellapi.h>
#include "SettingsManager.h"
#include "SliderPopup.h"

class TrayManager {
public:
    TrayManager(HWND hWndParent, HINSTANCE hInstance, SettingsManager& settings, SliderPopup& popup);
    ~TrayManager();

    bool Initialize();
    void Remove();
    void ShowContextMenu();
    void ShowBalloon(const wchar_t* title, const wchar_t* msg);
    void HandleMenuCommand(UINT cmdId);

    void OnTrayNotification(LPARAM lParam);

private:
    HWND m_hWndParent;
    HINSTANCE m_hInstance;
    SettingsManager& m_settings;
    SliderPopup& m_popup;
    NOTIFYICONDATAW m_nid;
    HMENU m_hMenu;
};
