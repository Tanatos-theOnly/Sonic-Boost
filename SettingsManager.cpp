#include "SettingsManager.h"
#include <shlwapi.h>

SettingsManager::SettingsManager()
    : m_soundLevel(100),
      m_boostIsEnabled(true),
      m_boostMethod(1),
      m_compressorIsEnabled(false),
      m_autoHide(true),
      m_startupTip(true),
      m_appAutoStart(false),
      m_initialVolume(0),
      m_regSubKey(L"Software\\SonicBoost\\Options")
{
    // Default hotkeys: Ctrl+Alt+F10 (Increase) and Ctrl+Alt+F9 (Decrease)
    m_hotkeyInc.enabled = true;
    m_hotkeyInc.vkCode = VK_F10;
    m_hotkeyInc.modifiers = MOD_CONTROL | MOD_ALT;

    m_hotkeyDec.enabled = true;
    m_hotkeyDec.vkCode = VK_F9;
    m_hotkeyDec.modifiers = MOD_CONTROL | MOD_ALT;
}

SettingsManager::~SettingsManager() {
}

void SettingsManager::ResetToDefaults() {
    m_soundLevel = 100;
    m_boostIsEnabled = true;
    m_boostMethod = 1;
    m_compressorIsEnabled = false;
    m_autoHide = true;
    m_startupTip = true;
    m_appAutoStart = false;
    m_initialVolume = 0;

    m_hotkeyInc.enabled = true;
    m_hotkeyInc.vkCode = VK_F10;
    m_hotkeyInc.modifiers = MOD_CONTROL | MOD_ALT;

    m_hotkeyDec.enabled = true;
    m_hotkeyDec.vkCode = VK_F9;
    m_hotkeyDec.modifiers = MOD_CONTROL | MOD_ALT;

    SetAutoStart(false);
    Save();
}

void SettingsManager::Load() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, m_regSubKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD dwType = REG_DWORD;
        DWORD dwValue = 0;
        DWORD dwSize = sizeof(DWORD);

        if (RegQueryValueExW(hKey, L"SoundLevel", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS) {
            SetSoundLevel((int)dwValue);
        }

        dwSize = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"BoostIsEnabled", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS) {
            m_boostIsEnabled = (dwValue != 0);
        }

        dwSize = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"BoostMethod", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS) {
            m_boostMethod = (int)dwValue;
        }

        dwSize = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"CompressorIsEnabled", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS) {
            m_compressorIsEnabled = (dwValue != 0);
        }

        dwSize = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"AutoHide", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS) {
            m_autoHide = (dwValue != 0);
        }

        dwSize = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"StartupTip", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS) {
            m_startupTip = (dwValue != 0);
        }

        dwSize = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"AppAutoStart", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS) {
            m_appAutoStart = (dwValue != 0);
        }

        dwSize = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"InitialVolume", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS) {
            m_initialVolume = (int)dwValue;
        }

        // Hotkeys binary
        BYTE hkBuf[8];
        dwSize = sizeof(hkBuf);
        dwType = REG_BINARY;
        if (RegQueryValueExW(hKey, L"AppHotKeys", NULL, &dwType, hkBuf, &dwSize) == ERROR_SUCCESS && dwSize == 8) {
            m_hotkeyInc.vkCode = hkBuf[0];
            m_hotkeyInc.modifiers = hkBuf[1];
            m_hotkeyInc.enabled = (hkBuf[2] != 0);

            m_hotkeyDec.vkCode = hkBuf[4];
            m_hotkeyDec.modifiers = hkBuf[5];
            m_hotkeyDec.enabled = (hkBuf[6] != 0);
        }

        RegCloseKey(hKey);
    }

    if (m_initialVolume != 0) {
        SetSoundLevel(m_initialVolume);
    }
}

void SettingsManager::Save() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, m_regSubKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD dwValue = (DWORD)m_soundLevel;
        RegSetValueExW(hKey, L"SoundLevel", 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(DWORD));

        dwValue = m_boostIsEnabled ? 1 : 0;
        RegSetValueExW(hKey, L"BoostIsEnabled", 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(DWORD));

        dwValue = (DWORD)m_boostMethod;
        RegSetValueExW(hKey, L"BoostMethod", 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(DWORD));

        dwValue = m_compressorIsEnabled ? 1 : 0;
        RegSetValueExW(hKey, L"CompressorIsEnabled", 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(DWORD));

        dwValue = m_autoHide ? 1 : 0;
        RegSetValueExW(hKey, L"AutoHide", 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(DWORD));

        dwValue = m_startupTip ? 1 : 0;
        RegSetValueExW(hKey, L"StartupTip", 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(DWORD));

        dwValue = m_appAutoStart ? 1 : 0;
        RegSetValueExW(hKey, L"AppAutoStart", 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(DWORD));

        dwValue = (DWORD)m_initialVolume;
        RegSetValueExW(hKey, L"InitialVolume", 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(DWORD));

        BYTE hkBuf[8] = {
            (BYTE)m_hotkeyInc.vkCode, (BYTE)m_hotkeyInc.modifiers, (BYTE)(m_hotkeyInc.enabled ? 1 : 0), 0,
            (BYTE)m_hotkeyDec.vkCode, (BYTE)m_hotkeyDec.modifiers, (BYTE)(m_hotkeyDec.enabled ? 1 : 0), 0
        };
        RegSetValueExW(hKey, L"AppHotKeys", 0, REG_BINARY, hkBuf, sizeof(hkBuf));

        RegCloseKey(hKey);
    }
}

void SettingsManager::SetAutoStart(bool autoStart) {
    m_appAutoStart = autoStart;
    HKEY hRunKey;
    const wchar_t* runPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, runPath, 0, KEY_WRITE, &hRunKey) == ERROR_SUCCESS) {
        if (autoStart) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            RegSetValueExW(hRunKey, L"SonicBoost", 0, REG_SZ, (const BYTE*)exePath, (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hRunKey, L"SonicBoost");
        }
        RegCloseKey(hRunKey);
    }
}
