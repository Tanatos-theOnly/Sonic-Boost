#pragma once
#include <windows.h>
#include "SettingsManager.h"
#include "AudioEngine.h"
#include "SliderPopup.h"

class SettingsDialog {
public:
    SettingsDialog(HINSTANCE hInstance, HWND hWndParent, SettingsManager& settings, SliderPopup& popup);
    ~SettingsDialog();

    void Show();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void InitControls();
    void SaveControls();
    void SetControlsFromSettings();

    HINSTANCE m_hInstance;
    HWND m_hWndParent;
    HWND m_hWnd;
    SettingsManager& m_settings;
    SliderPopup& m_popup;

    HWND m_hChkAutostart;
    HWND m_hChkStartupTip;
    HWND m_hChkAutoHide;
    HWND m_hCboInitialVol;

    HWND m_hChkInc;
    HWND m_hChkIncShift;
    HWND m_hChkIncCtrl;
    HWND m_hChkIncAlt;
    HWND m_hCboIncKey;

    HWND m_hChkDec;
    HWND m_hChkDecShift;
    HWND m_hChkDecCtrl;
    HWND m_hChkDecAlt;
    HWND m_hCboDecKey;
};
