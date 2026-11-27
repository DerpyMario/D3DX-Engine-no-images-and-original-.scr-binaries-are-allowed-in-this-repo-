//============================================================================================
// resources.h -- resource identifiers for Mystify.scr / Mystify.scr.mui
//
// Every ID below was recovered from the resource directory of the original binaries
// (Mystify.scr 10.0.19041.4355 x64 + Mystify.scr.mui en-US) and matches them 1:1.
//
//  .scr (language neutral)     .mui (en-US satellite)
//  ------------------------    -----------------------------------------------
//  "BMP" 102 (128x128 sprite)  DIALOG 105  "Mystify Settings"      (12 controls)
//  ICON  1, 2                  DIALOG 200  "Display Settings" mono (9 controls)
//  GROUP_ICON 101              DIALOG 201  "Display Settings" multi(14 controls)
//  VERSION 1                   STRING block 1    ID 1      "Mystify"
//  MANIFEST 1 (type 24)        STRING block 19   IDs 300..302
//  MUI 1 (RT_MUI config)       STRING block 132  IDs 2100..2111 (12 error texts)
//                              STRING block 133  ID  2112 ("No preview available")
//                              STRING block 138  IDs 2200..2207 (8 monitor texts)
//                              STRING block 139  IDs 2208..2212 (5 monitor texts)
//                              VERSION 1
//
// NOTE ON STRING IDs: the MUI loader maps LoadStringW(id) on the language-neutral
// .scr onto satellite RT_STRING block (id/16)+1, entry id%16. The satellite block
// numbering is therefore +1 versus the classic RT_STRING enumeration.
//============================================================================================
#pragma once

//----------------------------------------------------------------------------
// .scr side
//----------------------------------------------------------------------------
#define IDB_TEXTURE             102     // named type "BMP" id 102: 128x128 RLE-encoded sprite
#define IDI_ICON1               1       // RT_ICON  1  (16x16 4bpp frame)
#define IDI_ICON2               2       // RT_ICON  2  (32x32 8bpp frame)
#define IDI_APPICON             101     // RT_GROUP_ICON "Mystify"
#define VS_VERSION_INFO         1
#define IDR_MANIFEST            1       // RT_MANIFEST (type 24)

//----------------------------------------------------------------------------
// .mui side - dialogs
//----------------------------------------------------------------------------
#define IDD_SETTINGS                    105     // "Mystify Settings"
#define IDD_DISPLAY_SETTINGS_SINGLE     200     // "Display Settings" (single monitor)
#define IDD_DISPLAY_SETTINGS_MULTI      201     // "Display Settings" (multi-monitor, tabbed)

// IDD_SETTINGS control IDs (decoded from DLGTEMPLATEEX 105)
#define IDC_BTN_OK                      1       // "OK"
#define IDC_BTN_CANCEL                  2       // "Cancel"
#define IDC_BTN_DISPLAY_SETTINGS        1002    // "&Display Settings..."
#define IDC_TRK_NUM_LINES               1030    // group 1025 "Number of Lines"
#define IDC_TRK_LINE_WIDTH              1036    // group 1026 "Line Width"
#define IDC_TRK_CAMERA_FOV              1031    // group 1027 "Camera Field of View Angle"
#define IDC_CHK_SPAN_MONITORS           1020    // "Span Multiple Monitors"
#define IDC_CHK_BLUR                    1021    // "Apply Blur and Fade"
#define IDC_LBL_CREDIT                  1038    // "Screensaver by Stephen Coy"

// IDD_DISPLAY_SETTINGS_* control IDs (decoded from DLGTEMPLATEEX 200/201)
#define IDC_TAB_MONITORS                2000    // SysTabControl32 "Tab1" (dialog 201)
#define IDC_LBL_MONITOR                 2001    // "Monitor %d"          (dialog 201)
#define IDC_LBL_ADAPTER_NAME            2002    // adapter description
#define IDC_LBL_RENDERING_DESC          2003    // rendering capability text
#define IDC_BTN_HELP                    2004    // "&Help"
#define IDC_CHK_DISABLE_HW              2005    // "&Disable hardware 3D rendering"
#define IDC_GRP_MONITOR_USAGE           2006    // "Monitor usage"       (dialog 201)
#define IDC_RAD_DISPLAY_SAVER           2007    // "Display &screen saver on this monitor"
#define IDC_RAD_DISPLAY_NOTHING         2008    // "Display &nothing on this monitor"

//----------------------------------------------------------------------------
// .mui side - string IDs (language-neutral IDs as passed to LoadStringW)
//----------------------------------------------------------------------------
#define IDS_APP_NAME                    1       // "Mystify"
#define IDS_SAVER_TITLE                 300     // "Mystify Screen Saver"
#define IDS_NO_OPTIONS                  301     // "No options"
#define IDS_NO_OPTIONS_DESC             302     // "This screen saver has no options that you can set."

// HRESULT -> message map (vtable slot 8, RVA 0xB7E0 in Mystify.scr)
#define IDS_ERR_UNSPECIFIED             2100    // (fallback; "Unspecified error encountered.")
#define IDS_ERR_NO_DIRECT3D             2101
#define IDS_ERR_GENERIC_START           2102
#define IDS_ERR_COULD_NOT_START         2103
#define IDS_ERR_NEWER_CARD              2104
#define IDS_ERR_NO_HW_ACCEL             2105
#define IDS_ERR_FUNC_UNAVAILABLE_HW     2106
#define IDS_ERR_TRY_LOWER_RES           2107
#define IDS_ERR_NO_TEXTURES             2108
#define IDS_ERR_DESKTOP_RES             2109
#define IDS_ERR_NO_SYS_MEMORY           2110
#define IDS_ERR_NO_VIDEO_MEMORY         2111
#define IDS_NO_PREVIEW                  2112

// Per-monitor rendering descriptions (Display Settings dialogs)
#define IDS_MON_HW_OK                   2200
#define IDS_MON_HW_SW_FALLBACK          2201
#define IDS_MON_HW_UNSUPPORTED          2202
#define IDS_MON_HW_NO_SW                2203
#define IDS_MON_NO_HW_SW_OK             2204
#define IDS_MON_NO_HW_SW_UNSUPPORTED    2205
#define IDS_MON_NO_HW_NO_SW             2206
#define IDS_MON_HW_DISABLED_SW_OK       2207
#define IDS_MON_HW_DISABLED_SW_UNSUPPORTED 2208
#define IDS_MON_HW_DISABLED_NO_SW       2209
#define IDS_MON_CAN_HW                  2210
#define IDS_MON_CAN_SW                  2211
#define IDS_MON_CANNOT                  2212
