#include "TrayManager.h"
#include "resource.h"

TrayManager::TrayManager(HWND hWndParent, HINSTANCE hInstance, SettingsManager& settings, SliderPopup& popup)
    : m_hWndParent(hWndParent),
      m_hInstance(hInstance),
      m_settings(settings),
      m_popup(popup),
      m_hMenu(NULL)
{
    ZeroMemory(&m_nid, sizeof(m_nid));
}

TrayManager::~TrayManager() {
    Remove();
    if (m_hMenu) {
        DestroyMenu(m_hMenu);
    }
}

bool TrayManager::Initialize() {
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hWndParent;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON_NOTIFY;

    HICON hIcon = (HICON)LoadImageW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON),
                                   IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                   LR_DEFAULTCOLOR);
    if (!hIcon) {
        hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    }
    if (!hIcon) {
        hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    m_nid.hIcon = hIcon;
    wcscpy_s(m_nid.szTip, L"SonicBoost");

    // Remove any ghost icon from previous abnormal termination
    Shell_NotifyIconW(NIM_DELETE, &m_nid);

    BOOL bAdded = FALSE;
    for (int retry = 0; retry < 5; ++retry) {
        if (Shell_NotifyIconW(NIM_ADD, &m_nid)) {
            bAdded = TRUE;
            break;
        }
        Sleep(100);
    }

    if (!bAdded) {
        return false;
    }

    if (m_settings.IsStartupTip()) {
        ShowBalloon(L"SonicBoost", L"Click on the icon to adjust volume or right click for settings.");
    }

    return true;
}

void TrayManager::Remove() {
    if (m_nid.cbSize) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_nid.cbSize = 0;
    }
}

void TrayManager::ShowBalloon(const wchar_t* title, const wchar_t* msg) {
    m_nid.uFlags |= NIF_INFO;
    wcscpy_s(m_nid.szInfoTitle, title);
    wcscpy_s(m_nid.szInfo, msg);
    m_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    m_nid.uFlags &= ~NIF_INFO;
}

void TrayManager::ShowContextMenu() {
    if (m_hMenu) {
        DestroyMenu(m_hMenu);
    }

    m_hMenu = CreatePopupMenu();

    AppendMenuW(m_hMenu, MF_STRING, ID_BOOSTER_SHOW, L"Show");
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m_hMenu, MF_STRING, ID_BOOSTER_SETTINGS, L"Settings");

    // Submenu: Boost Mode
    HMENU hSubMenu = CreatePopupMenu();
    AppendMenuW(hSubMenu, MF_STRING | (m_settings.GetBoostMethod() == 1 ? MF_CHECKED : MF_UNCHECKED),
                ID_BOOST_METHOD_HOOK, L"Injection");
    AppendMenuW(hSubMenu, MF_STRING | (m_settings.GetBoostMethod() == 2 ? MF_CHECKED : MF_UNCHECKED),
                ID_BOOST_METHOD_HOOK_APO, L"Injection and APO effect");
    AppendMenuW(hSubMenu, MF_STRING | (m_settings.GetBoostMethod() == 3 ? MF_CHECKED : MF_UNCHECKED),
                ID_BOOST_METHOD_APO, L"APO effect");
    AppendMenuW(hSubMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hSubMenu, MF_STRING | (m_settings.IsCompressorEnabled() ? MF_CHECKED : MF_UNCHECKED),
                ID_USE_COMPRESSOR, L"SuperBoost");

    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hSubMenu, L"Boost Mode");
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m_hMenu, MF_STRING, ID_HELP_ABOUT, L"About...");
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m_hMenu, MF_STRING, ID_BOOSTER_EXIT, L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hWndParent);
    TrackPopupMenu(m_hMenu, TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWndParent, NULL);
    PostMessageW(m_hWndParent, WM_NULL, 0, 0);
}

void TrayManager::OnTrayNotification(LPARAM lParam) {
    switch (lParam) {
    case WM_LBUTTONUP: {
        POINT pt;
        GetCursorPos(&pt);
        m_popup.Show(pt);
        break;
    }
    case WM_RBUTTONUP:
        ShowContextMenu();
        break;
    }
}
