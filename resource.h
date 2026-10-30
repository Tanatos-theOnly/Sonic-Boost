#pragma once

#define IDI_APP_ICON                    101
#define IDD_SOUNDBOOSTER_DIALOG         102
#define IDD_SETTINGS_DIALOG             32774

// Controls in Slider Popup (Dialog 102)
#define IDC_SLIDER_VOLUME               1001
#define IDC_CHB_ENABLE                  1002

// Controls in Settings Dialog (Dialog 32774)
#define IDC_CHB_AUTOSTART               1003
#define IDC_GROUPBOX_LANGUAGE           1004
#define IDC_CHB_HOTKEY_INC              1005
#define IDC_CHB_INC_SHIFT               1006
#define IDC_CHB_INC_CTRL                1007
#define IDC_CHB_INC_ALT                 1008
#define IDC_EDT_INC_KEY                 1009
#define IDC_CHB_HOTKEY_DEC              1010
#define IDC_CHB_DEC_SHIFT               1011
#define IDC_CHB_DEC_CTRL                1012
#define IDC_CHB_DEC_ALT                 1013
#define IDC_EDT_DEC_KEY                 1014
#define IDC_BTN_DEFAULTS                1021
#define IDC_CHB_STARTUP_TIP             1022
#define IDC_CHB_UPDATE_CHECK            1026
#define IDC_CBO_BOOST_LEVEL             1028
#define IDC_CHB_AUTOHIDE                1062
#define IDC_CHB_FIX_ISSUES              1063

// Tray Menu Commands
#define ID_BOOSTER_SHOW                 32772
#define ID_BOOSTER_EXIT                 32773
#define ID_BOOSTER_SETTINGS             32774
#define ID_HELP_ABOUT                   32775
#define ID_HELP_BUYNOW                  32776
#define ID_HELP_MAIN                    32777
#define ID_HELP_VISITHOMEPAGE           32778
#define ID_HELP_CHECKFORUPDATES         32779
#define ID_HELP_ACTIVATE                32780
#define ID_HELP_DEACTIVATE              32781

#define ID_BOOST_METHOD_HOOK            32784
#define ID_BOOST_METHOD_HOOK_APO        32785
#define ID_BOOST_METHOD_APO             32786
#define ID_USE_COMPRESSOR               32787

// Custom Messages
#define WM_TRAYICON_NOTIFY              (WM_USER + 101)
#define WM_SLIDER_UPDATED               (WM_USER + 102)
