//-----------------------------------------------------------------------------
// File: d3dsaver9res.h
//
// Desc: Resource identifiers shared by every screen saver that is built on
//       D3DSaver9.lib.  The numeric values are the ones found in the shipped
//       binaries (and in their .mui satellites).
//-----------------------------------------------------------------------------
#ifndef _D3DSAVER9RES_H
#define _D3DSAVER9RES_H

// Screen saver name (string 1 is what the Display control panel shows)
#define IDS_DESCRIPTION                 1

// Icon
#define IDI_MAIN_ICON                   101

// Display-settings dialogs
#define IDD_SINGLEMONITORSETTINGS       200
#define IDD_MULTIMONITORSETTINGS        201

#define IDC_MONITORSTAB                 2000
#define IDC_TABNAMEFMT                  2001
#define IDC_ADAPTERNAME                 2002
#define IDC_RENDERING                   2003
#define IDC_MOREINFO                    2004
#define IDC_DISABLEHW                   2005
#define IDC_SCREENUSAGEBOX              2006
#define IDC_RENDER                      2007
#define IDC_LEAVEBLACK                  2008
#define IDC_DISPLAYMODEBOX              2009
#define IDC_MODESSTATIC                 2010
#define IDC_MODESCOMBO                  2011
#define IDC_AUTOMATIC                   2012
#define IDC_DISPLAYMODENOTE             2013
#define IDC_GENERALBOX                  2014
#define IDC_SAME                        2015
#define IDC_MODEFMT                     2016

// Error strings
#define IDS_ERR_GENERIC                 2100
#define IDS_ERR_NODIRECT3D              2101
#define IDS_ERR_NOWINDOWEDHAL           2102
#define IDS_ERR_CREATEDEVICEFAILED      2103
#define IDS_ERR_NOCOMPATIBLEDEVICES     2104
#define IDS_ERR_NOHARDWAREDEVICE        2105
#define IDS_ERR_HALNOTCOMPATIBLE        2106
#define IDS_ERR_NOHALTHISMODE           2107
#define IDS_ERR_MEDIANOTFOUND           2108
#define IDS_ERR_RESIZEFAILED            2109
#define IDS_ERR_OUTOFMEMORY             2110
#define IDS_ERR_OUTOFVIDEOMEMORY        2111
#define IDS_ERR_NOPREVIEW               2112

// "More information" strings of the display-settings dialog
#define IDS_INFO_GOODHAL                2200
#define IDS_INFO_BADHAL_GOODSW          2201
#define IDS_INFO_BADHAL_BADSW           2202
#define IDS_INFO_BADHAL_NOSW            2203
#define IDS_INFO_NOHAL_GOODSW           2204
#define IDS_INFO_NOHAL_BADSW            2205
#define IDS_INFO_NOHAL_NOSW             2206
#define IDS_INFO_DISABLEDHAL_GOODSW     2207
#define IDS_INFO_DISABLEDHAL_BADSW      2208
#define IDS_INFO_DISABLEDHAL_NOSW       2209
#define IDS_RENDERING_HAL               2210
#define IDS_RENDERING_SW                2211
#define IDS_RENDERING_NONE              2212

//-----------------------------------------------------------------------------
// Window classes for the CONTROL statements of the dialog templates.  The
// templates of the originals name the predefined control classes by ordinal
// (0x0080 = Button, ...).  rc.exe turns the class names into these ordinals
// by itself; windres stores an ordinal only when given one.  (llvm-rc takes
// names only, and keeps them as names.  windres also stores the names of the
// other classes, such as "msctls_trackbar32", in upper case.)
//-----------------------------------------------------------------------------
#if defined(__GNUC__) && !defined(__clang__)
#define CTL_BUTTON                      0x0080
#define CTL_EDIT                        0x0081
#define CTL_STATIC                      0x0082
#define CTL_COMBOBOX                    0x0085
#else
#define CTL_BUTTON                      "Button"
#define CTL_EDIT                        "Edit"
#define CTL_STATIC                      "Static"
#define CTL_COMBOBOX                    "ComboBox"
#endif

#endif // _D3DSAVER9RES_H
