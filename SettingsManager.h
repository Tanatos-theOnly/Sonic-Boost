#pragma once
#include <windows.h>
#include <string>

struct HotkeyConfig {
    bool enabled;
    UINT vkCode;
    UINT modifiers; // MOD_CONTROL, MOD_SHIFT, MOD_ALT
};

class SettingsManager {
public:
    SettingsManager();
    ~SettingsManager();

    void Load();
    void Save();
    void ResetToDefaults();

    // Accessors
    int GetSoundLevel() const { return m_soundLevel; }
    void SetSoundLevel(int level) { m_soundLevel = (level < 100 ? 100 : (level > 500 ? 500 : level)); }

    bool IsBoostEnabled() const { return m_boostIsEnabled; }
    void SetBoostEnabled(bool enabled) { m_boostIsEnabled = enabled; }

    int GetBoostMethod() const { return m_boostMethod; }
    void SetBoostMethod(int method) { m_boostMethod = method; }

    bool IsCompressorEnabled() const { return m_compressorIsEnabled; }
    void SetCompressorEnabled(bool enabled) { m_compressorIsEnabled = enabled; }

    bool IsAutoHide() const { return m_autoHide; }
    void SetAutoHide(bool autoHide) { m_autoHide = autoHide; }

    bool IsStartupTip() const { return m_startupTip; }
    void SetStartupTip(bool tip) { m_startupTip = tip; }

    bool IsAutoStart() const { return m_appAutoStart; }
    void SetAutoStart(bool autoStart);

    int GetInitialVolume() const { return m_initialVolume; }
    void SetInitialVolume(int vol) { m_initialVolume = vol; }

    HotkeyConfig GetHotkeyInc() const { return m_hotkeyInc; }
    void SetHotkeyInc(const HotkeyConfig& hk) { m_hotkeyInc = hk; }

    HotkeyConfig GetHotkeyDec() const { return m_hotkeyDec; }
    void SetHotkeyDec(const HotkeyConfig& hk) { m_hotkeyDec = hk; }

private:
    int m_soundLevel;
    bool m_boostIsEnabled;
    int m_boostMethod;
    bool m_compressorIsEnabled;
    bool m_autoHide;
    bool m_startupTip;
    bool m_appAutoStart;
    int m_initialVolume;

    HotkeyConfig m_hotkeyInc;
    HotkeyConfig m_hotkeyDec;

    const wchar_t* m_regSubKey;
};
