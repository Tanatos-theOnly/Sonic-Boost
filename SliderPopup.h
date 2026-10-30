#pragma once
#include <windows.h>
#include <commctrl.h>
#include "SettingsManager.h"
#include "AudioEngine.h"

class SliderPopup {
public:
    SliderPopup(HINSTANCE hInstance, SettingsManager& settings, AudioEngine& audio);
    ~SliderPopup();

    bool Create();
    void Show(const POINT& ptTray);
    void Hide();
    bool IsVisible() const;

    HWND GetHwnd() const { return m_hWnd; }
    void UpdateSliderFromSettings();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void PositionNearTray(const POINT& ptTray);
    void UpdateTooltip(int value);

    HINSTANCE m_hInstance;
    SettingsManager& m_settings;
    AudioEngine& m_audio;

    HWND m_hWnd;
    HWND m_hSlider;
    HWND m_hChkEnable;
    HWND m_hTooltip;
    TOOLINFOW m_toolInfo;

    bool m_isTrackingThumb;
};
