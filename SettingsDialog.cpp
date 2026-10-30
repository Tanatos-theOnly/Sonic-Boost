#include "SettingsDialog.h"
#include "resource.h"
#include <commctrl.h>

static const wchar_t* SETTINGS_CLASS_NAME = L"SoundBoosterSettingsDialogClass";

SettingsDialog::SettingsDialog(HINSTANCE hInstance, HWND hWndParent, SettingsManager& settings, SliderPopup& popup)
    : m_hInstance(hInstance),
      m_hWndParent(hWndParent),
      m_hWnd(NULL),
      m_settings(settings),
      m_popup(popup),
      m_hChkAutostart(NULL),
      m_hChkStartupTip(NULL),
      m_hChkAutoHide(NULL),
      m_hCboInitialVol(NULL),
      m_hChkInc(NULL),
      m_hChkIncShift(NULL),
      m_hChkIncCtrl(NULL),
      m_hChkIncAlt(NULL),
      m_hCboIncKey(NULL),
      m_hChkDec(NULL),
      m_hChkDecShift(NULL),
      m_hChkDecCtrl(NULL),
      m_hChkDecAlt(NULL),
      m_hCboDecKey(NULL)
{
}

SettingsDialog::~SettingsDialog() {
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
    }
}

void SettingsDialog::Show() {
    if (m_hWnd && IsWindow(m_hWnd)) {
        SetForegroundWindow(m_hWnd);
        return;
    }

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SettingsDialog::WndProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = SETTINGS_CLASS_NAME;

    RegisterClassExW(&wc);

    // Center window on desktop
    RECT rcDesktop;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcDesktop, 0);
    int width = 420;
    int height = 480;
    int x = (rcDesktop.right - width) / 2;
    int y = (rcDesktop.bottom - height) / 2;

    m_hWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        SETTINGS_CLASS_NAME,
        L"SonicBoost Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, width, height,
        m_hWndParent, NULL, m_hInstance, this
    );

    InitControls();
    SetControlsFromSettings();
    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);
}

void SettingsDialog::InitControls() {
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    // Group 1: General Options
    HWND hGrpGeneral = CreateWindowExW(0, L"BUTTON", L"General Options",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 15, 10, 375, 130, m_hWnd, NULL, m_hInstance, NULL);
    SendMessageW(hGrpGeneral, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hChkAutostart = CreateWindowExW(0, L"BUTTON", L"Start on Windows startup",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 30, 35, 340, 20, m_hWnd, (HMENU)IDC_CHB_AUTOSTART, m_hInstance, NULL);
    SendMessageW(m_hChkAutostart, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hChkStartupTip = CreateWindowExW(0, L"BUTTON", L"Show startup notification",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 30, 60, 340, 20, m_hWnd, (HMENU)IDC_CHB_STARTUP_TIP, m_hInstance, NULL);
    SendMessageW(m_hChkStartupTip, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hChkAutoHide = CreateWindowExW(0, L"BUTTON", L"Automatically hide boost controls",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 30, 85, 340, 20, m_hWnd, (HMENU)IDC_CHB_AUTOHIDE, m_hInstance, NULL);
    SendMessageW(m_hChkAutoHide, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hLblVol = CreateWindowExW(0, L"STATIC", L"Boost level on startup:",
        WS_CHILD | WS_VISIBLE, 30, 110, 150, 20, m_hWnd, NULL, m_hInstance, NULL);
    SendMessageW(hLblVol, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hCboInitialVol = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP, 185, 107, 180, 150, m_hWnd, (HMENU)IDC_CBO_BOOST_LEVEL, m_hInstance, NULL);
    SendMessageW(m_hCboInitialVol, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(m_hCboInitialVol, CB_ADDSTRING, 0, (LPARAM)L"Use from last time");
    SendMessageW(m_hCboInitialVol, CB_ADDSTRING, 0, (LPARAM)L"100 % (No boost)");
    SendMessageW(m_hCboInitialVol, CB_ADDSTRING, 0, (LPARAM)L"200 %");
    SendMessageW(m_hCboInitialVol, CB_ADDSTRING, 0, (LPARAM)L"300 %");
    SendMessageW(m_hCboInitialVol, CB_ADDSTRING, 0, (LPARAM)L"400 %");
    SendMessageW(m_hCboInitialVol, CB_ADDSTRING, 0, (LPARAM)L"500 % (Maximum)");

    // Group 2: Hotkeys
    HWND hGrpHotkeys = CreateWindowExW(0, L"BUTTON", L"Volume Hotkeys",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 15, 150, 375, 220, m_hWnd, NULL, m_hInstance, NULL);
    SendMessageW(hGrpHotkeys, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Increase Hotkey
    m_hChkInc = CreateWindowExW(0, L"BUTTON", L"Increase volume hotkey:",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 30, 175, 340, 20, m_hWnd, (HMENU)IDC_CHB_HOTKEY_INC, m_hInstance, NULL);
    SendMessageW(m_hChkInc, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hChkIncCtrl = CreateWindowExW(0, L"BUTTON", L"Ctrl",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 50, 200, 50, 20, m_hWnd, (HMENU)IDC_CHB_INC_CTRL, m_hInstance, NULL);
    SendMessageW(m_hChkIncCtrl, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hChkIncAlt = CreateWindowExW(0, L"BUTTON", L"Alt",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 110, 200, 50, 20, m_hWnd, (HMENU)IDC_CHB_INC_ALT, m_hInstance, NULL);
    SendMessageW(m_hChkIncAlt, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hChkIncShift = CreateWindowExW(0, L"BUTTON", L"Shift",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 170, 200, 60, 20, m_hWnd, (HMENU)IDC_CHB_INC_SHIFT, m_hInstance, NULL);
    SendMessageW(m_hChkIncShift, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hCboIncKey = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP, 240, 198, 120, 150, m_hWnd, (HMENU)IDC_EDT_INC_KEY, m_hInstance, NULL);
    SendMessageW(m_hCboIncKey, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Decrease Hotkey
    m_hChkDec = CreateWindowExW(0, L"BUTTON", L"Decrease volume hotkey:",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 30, 250, 340, 20, m_hWnd, (HMENU)IDC_CHB_HOTKEY_DEC, m_hInstance, NULL);
    SendMessageW(m_hChkDec, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hChkDecCtrl = CreateWindowExW(0, L"BUTTON", L"Ctrl",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 50, 275, 50, 20, m_hWnd, (HMENU)IDC_CHB_DEC_CTRL, m_hInstance, NULL);
    SendMessageW(m_hChkDecCtrl, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hChkDecAlt = CreateWindowExW(0, L"BUTTON", L"Alt",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 110, 275, 50, 20, m_hWnd, (HMENU)IDC_CHB_DEC_ALT, m_hInstance, NULL);
    SendMessageW(m_hChkDecAlt, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hChkDecShift = CreateWindowExW(0, L"BUTTON", L"Shift",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 170, 275, 60, 20, m_hWnd, (HMENU)IDC_CHB_DEC_SHIFT, m_hInstance, NULL);
    SendMessageW(m_hChkDecShift, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hCboDecKey = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP, 240, 273, 120, 150, m_hWnd, (HMENU)IDC_EDT_DEC_KEY, m_hInstance, NULL);
    SendMessageW(m_hCboDecKey, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Populate F-keys in both comboboxes
    for (int i = 1; i <= 12; ++i) {
        wchar_t szKey[16];
        swprintf_s(szKey, L"F%d", i);
        SendMessageW(m_hCboIncKey, CB_ADDSTRING, 0, (LPARAM)szKey);
        SendMessageW(m_hCboDecKey, CB_ADDSTRING, 0, (LPARAM)szKey);
    }

    // Buttons
    HWND hBtnDefaults = CreateWindowExW(0, L"BUTTON", L"Defaults",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 20, 395, 90, 28, m_hWnd, (HMENU)IDC_BTN_DEFAULTS, m_hInstance, NULL);
    SendMessageW(hBtnDefaults, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hBtnOK = CreateWindowExW(0, L"BUTTON", L"OK",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, 205, 395, 85, 28, m_hWnd, (HMENU)IDOK, m_hInstance, NULL);
    SendMessageW(hBtnOK, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hBtnCancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 305, 395, 85, 28, m_hWnd, (HMENU)IDCANCEL, m_hInstance, NULL);
    SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);
}

void SettingsDialog::SetControlsFromSettings() {
    SendMessageW(m_hChkAutostart, BM_SETCHECK, m_settings.IsAutoStart() ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_hChkStartupTip, BM_SETCHECK, m_settings.IsStartupTip() ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_hChkAutoHide, BM_SETCHECK, m_settings.IsAutoHide() ? BST_CHECKED : BST_UNCHECKED, 0);

    int initVol = m_settings.GetInitialVolume();
    int idx = 0;
    if (initVol == 100) idx = 1;
    else if (initVol == 200) idx = 2;
    else if (initVol == 300) idx = 3;
    else if (initVol == 400) idx = 4;
    else if (initVol == 500) idx = 5;
    SendMessageW(m_hCboInitialVol, CB_SETCURSEL, idx, 0);

    // Inc Hotkey
    HotkeyConfig hkInc = m_settings.GetHotkeyInc();
    SendMessageW(m_hChkInc, BM_SETCHECK, hkInc.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_hChkIncCtrl, BM_SETCHECK, (hkInc.modifiers & MOD_CONTROL) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_hChkIncAlt, BM_SETCHECK, (hkInc.modifiers & MOD_ALT) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_hChkIncShift, BM_SETCHECK, (hkInc.modifiers & MOD_SHIFT) ? BST_CHECKED : BST_UNCHECKED, 0);
    if (hkInc.vkCode >= VK_F1 && hkInc.vkCode <= VK_F12) {
        SendMessageW(m_hCboIncKey, CB_SETCURSEL, hkInc.vkCode - VK_F1, 0);
    } else {
        SendMessageW(m_hCboIncKey, CB_SETCURSEL, 9, 0); // F10 default
    }

    // Dec Hotkey
    HotkeyConfig hkDec = m_settings.GetHotkeyDec();
    SendMessageW(m_hChkDec, BM_SETCHECK, hkDec.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_hChkDecCtrl, BM_SETCHECK, (hkDec.modifiers & MOD_CONTROL) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_hChkDecAlt, BM_SETCHECK, (hkDec.modifiers & MOD_ALT) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_hChkDecShift, BM_SETCHECK, (hkDec.modifiers & MOD_SHIFT) ? BST_CHECKED : BST_UNCHECKED, 0);
    if (hkDec.vkCode >= VK_F1 && hkDec.vkCode <= VK_F12) {
        SendMessageW(m_hCboDecKey, CB_SETCURSEL, hkDec.vkCode - VK_F1, 0);
    } else {
        SendMessageW(m_hCboDecKey, CB_SETCURSEL, 8, 0); // F9 default
    }
}

void SettingsDialog::SaveControls() {
    m_settings.SetAutoStart(SendMessageW(m_hChkAutostart, BM_GETCHECK, 0, 0) == BST_CHECKED);
    m_settings.SetStartupTip(SendMessageW(m_hChkStartupTip, BM_GETCHECK, 0, 0) == BST_CHECKED);
    m_settings.SetAutoHide(SendMessageW(m_hChkAutoHide, BM_GETCHECK, 0, 0) == BST_CHECKED);

    int idx = (int)SendMessageW(m_hCboInitialVol, CB_GETCURSEL, 0, 0);
    int volTable[] = { 0, 100, 200, 300, 400, 500 };
    m_settings.SetInitialVolume(idx >= 0 && idx < 6 ? volTable[idx] : 0);

    HotkeyConfig hkInc;
    hkInc.enabled = (SendMessageW(m_hChkInc, BM_GETCHECK, 0, 0) == BST_CHECKED);
    hkInc.modifiers = 0;
    if (SendMessageW(m_hChkIncCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED) hkInc.modifiers |= MOD_CONTROL;
    if (SendMessageW(m_hChkIncAlt, BM_GETCHECK, 0, 0) == BST_CHECKED) hkInc.modifiers |= MOD_ALT;
    if (SendMessageW(m_hChkIncShift, BM_GETCHECK, 0, 0) == BST_CHECKED) hkInc.modifiers |= MOD_SHIFT;
    int incKeyIdx = (int)SendMessageW(m_hCboIncKey, CB_GETCURSEL, 0, 0);
    hkInc.vkCode = (incKeyIdx >= 0 && incKeyIdx < 12) ? (VK_F1 + incKeyIdx) : VK_F10;
    m_settings.SetHotkeyInc(hkInc);

    HotkeyConfig hkDec;
    hkDec.enabled = (SendMessageW(m_hChkDec, BM_GETCHECK, 0, 0) == BST_CHECKED);
    hkDec.modifiers = 0;
    if (SendMessageW(m_hChkDecCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED) hkDec.modifiers |= MOD_CONTROL;
    if (SendMessageW(m_hChkDecAlt, BM_GETCHECK, 0, 0) == BST_CHECKED) hkDec.modifiers |= MOD_ALT;
    if (SendMessageW(m_hChkDecShift, BM_GETCHECK, 0, 0) == BST_CHECKED) hkDec.modifiers |= MOD_SHIFT;
    int decKeyIdx = (int)SendMessageW(m_hCboDecKey, CB_GETCURSEL, 0, 0);
    hkDec.vkCode = (decKeyIdx >= 0 && decKeyIdx < 12) ? (VK_F1 + decKeyIdx) : VK_F9;
    m_settings.SetHotkeyDec(hkDec);

    m_settings.Save();
}

LRESULT CALLBACK SettingsDialog::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsDialog* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCS = (CREATESTRUCT*)lParam;
        pThis = (SettingsDialog*)pCS->lpCreateParams;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (SettingsDialog*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(hWnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT SettingsDialog::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDOK) {
            SaveControls();
            DestroyWindow(m_hWnd);
            m_hWnd = NULL;
            return 0;
        } else if (id == IDCANCEL) {
            DestroyWindow(m_hWnd);
            m_hWnd = NULL;
            return 0;
        } else if (id == IDC_BTN_DEFAULTS) {
            m_settings.ResetToDefaults();
            SetControlsFromSettings();
            m_popup.UpdateSliderFromSettings();
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        DestroyWindow(m_hWnd);
        m_hWnd = NULL;
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
