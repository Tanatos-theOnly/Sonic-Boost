#include "SliderPopup.h"
#include "resource.h"
#include <cwchar>

static const wchar_t* POPUP_CLASS_NAME = L"SonicBoostSliderPopupClass";

SliderPopup::SliderPopup(HINSTANCE hInstance, SettingsManager& settings, AudioEngine& audio)
    : m_hInstance(hInstance),
      m_settings(settings),
      m_audio(audio),
      m_hWnd(NULL),
      m_hSlider(NULL),
      m_hChkEnable(NULL),
      m_hTooltip(NULL),
      m_isTrackingThumb(false)
{
    ZeroMemory(&m_toolInfo, sizeof(m_toolInfo));
}

SliderPopup::~SliderPopup() {
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
    }
}

bool SliderPopup::Create() {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    wc.lpfnWndProc = SliderPopup::WndProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = POPUP_CLASS_NAME;

    RegisterClassExW(&wc);

    // Create popup window with dialog-style borders
    m_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        POPUP_CLASS_NAME,
        L"SonicBoost",
        WS_POPUP | WS_BORDER,
        0, 0, 85, 230,
        NULL, NULL, m_hInstance, this
    );

    if (!m_hWnd) return false;

    // Create vertical trackbar
    m_hSlider = CreateWindowExW(
        0, TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_VERT | TBS_AUTOTICKS | TBS_BOTH | WS_TABSTOP,
        15, 10, 55, 175,
        m_hWnd, (HMENU)IDC_SLIDER_VOLUME, m_hInstance, NULL
    );

    // Trackbar range -500 to -100 (top is -500 / 500%, bottom is -100 / 100%)
    SendMessageW(m_hSlider, TBM_SETRANGEMIN, FALSE, (LPARAM)-500);
    SendMessageW(m_hSlider, TBM_SETRANGEMAX, TRUE, (LPARAM)-100);
    SendMessageW(m_hSlider, TBM_SETLINESIZE, 0, (LPARAM)10);
    SendMessageW(m_hSlider, TBM_SETPAGESIZE, 0, (LPARAM)20);
    SendMessageW(m_hSlider, TBM_SETTICFREQ, 50, 0);

    // Create Enable checkbox
    m_hChkEnable = CreateWindowExW(
        0, L"BUTTON", L"Enable",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        14, 195, 60, 20,
        m_hWnd, (HMENU)IDC_CHB_ENABLE, m_hInstance, NULL
    );

    // Create tooltip
    m_hTooltip = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        m_hWnd, NULL, m_hInstance, NULL
    );

    m_toolInfo.cbSize = sizeof(TOOLINFOW);
    m_toolInfo.uFlags = TTF_SUBCLASS | TTF_TRACK | TTF_ABSOLUTE;
    m_toolInfo.hwnd = m_hWnd;
    m_toolInfo.uId = (UINT_PTR)m_hSlider;
    m_toolInfo.hinst = m_hInstance;
    m_toolInfo.lpszText = const_cast<LPWSTR>(L"");
    SendMessageW(m_hTooltip, TTM_ADDTOOLW, 0, (LPARAM)&m_toolInfo);

    UpdateSliderFromSettings();
    return true;
}

void SliderPopup::UpdateSliderFromSettings() {
    int level = m_settings.GetSoundLevel();
    SendMessageW(m_hSlider, TBM_SETPOS, TRUE, (LPARAM)-level);
    SendMessageW(m_hChkEnable, BM_SETCHECK, m_settings.IsBoostEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
    m_audio.UpdateVolume(level);
    m_audio.SetEnabled(m_settings.IsBoostEnabled());
    m_audio.SetSuperBoost(m_settings.IsCompressorEnabled());
}

void SliderPopup::PositionNearTray(const POINT& ptTray) {
    RECT rcScreen;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);

    RECT rcWindow;
    GetWindowRect(m_hWnd, &rcWindow);
    int width = rcWindow.right - rcWindow.left;
    int height = rcWindow.bottom - rcWindow.top;

    int x = ptTray.x - width / 2;
    int y = ptTray.y - height - 10;

    // Screen boundary adjustments
    if (x + width > rcScreen.right) x = rcScreen.right - width - 5;
    if (x < rcScreen.left) x = rcScreen.left + 5;
    if (y < rcScreen.top) y = ptTray.y + 10;
    if (y + height > rcScreen.bottom) y = rcScreen.bottom - height - 5;

    SetWindowPos(m_hWnd, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);
}

void SliderPopup::Show(const POINT& ptTray) {
    PositionNearTray(ptTray);
    ShowWindow(m_hWnd, SW_SHOW);
    SetForegroundWindow(m_hWnd);
    SetFocus(m_hSlider);
}

void SliderPopup::Hide() {
    ShowWindow(m_hWnd, SW_HIDE);
    SendMessageW(m_hTooltip, TTM_TRACKACTIVATE, FALSE, (LPARAM)&m_toolInfo);
}

bool SliderPopup::IsVisible() const {
    return (m_hWnd && IsWindowVisible(m_hWnd));
}

void SliderPopup::UpdateTooltip(int value) {
    wchar_t szTip[32];
    swprintf_s(szTip, L" %d %%", value);
    m_toolInfo.lpszText = szTip;
    SendMessageW(m_hTooltip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&m_toolInfo);

    RECT rcSlider;
    GetWindowRect(m_hSlider, &rcSlider);

    // Calculate thumb position vertically
    int minPos = -500;
    int maxPos = -100;
    float ratio = (float)(-value - minPos) / (float)(maxPos - minPos);
    int thumbY = rcSlider.top + (int)(ratio * (rcSlider.bottom - rcSlider.top - 20)) + 10;
    int thumbX = rcSlider.right + 8;

    SendMessageW(m_hTooltip, TTM_TRACKPOSITION, 0, MAKELPARAM(thumbX, thumbY));
    SendMessageW(m_hTooltip, TTM_TRACKACTIVATE, TRUE, (LPARAM)&m_toolInfo);
}

LRESULT CALLBACK SliderPopup::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SliderPopup* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCS = (CREATESTRUCT*)lParam;
        pThis = (SliderPopup*)pCS->lpCreateParams;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (SliderPopup*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(hWnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT SliderPopup::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_VSCROLL: {
        int pos = (int)SendMessageW(m_hSlider, TBM_GETPOS, 0, 0);
        int volPercent = -pos; // convert negative trackbar value to positive percentage (100 to 500)
        m_settings.SetSoundLevel(volPercent);
        m_settings.Save();
        m_audio.UpdateVolume(volPercent);

        UpdateTooltip(volPercent);

        int scrollCode = LOWORD(wParam);
        if (scrollCode == TB_ENDTRACK) {
            // Dismiss tooltip after release
            SetTimer(hWnd, 1, 1500, NULL);
        }
        return 0;
    }

    case WM_TIMER:
        if (wParam == 1) {
            KillTimer(hWnd, 1);
            SendMessageW(m_hTooltip, TTM_TRACKACTIVATE, FALSE, (LPARAM)&m_toolInfo);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_CHB_ENABLE) {
            bool checked = (SendMessageW(m_hChkEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
            m_settings.SetBoostEnabled(checked);
            m_settings.Save();
            m_audio.SetEnabled(checked);
            return 0;
        }
        break;

    case WM_MOUSEWHEEL: {
        short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        int current = m_settings.GetSoundLevel();
        if (zDelta > 0) current += 10;
        else if (zDelta < 0) current -= 10;
        m_settings.SetSoundLevel(current);
        m_settings.Save();
        UpdateSliderFromSettings();
        UpdateTooltip(m_settings.GetSoundLevel());
        SetTimer(hWnd, 1, 1500, NULL);
        return 0;
    }

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && m_settings.IsAutoHide()) {
            Hide();
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            Hide();
            return 0;
        }
        break;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
