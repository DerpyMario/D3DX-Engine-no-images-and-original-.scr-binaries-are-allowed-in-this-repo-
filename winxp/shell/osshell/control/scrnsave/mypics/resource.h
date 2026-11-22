//-----------------------------------------------------------------------------
// File: resource.h
//
// Desc: Resource identifiers of the My Pictures Slideshow screen saver
//       (ssmypics.scr).  The values are those of the original binary; the
//       names are chosen by the reconstruction.
//-----------------------------------------------------------------------------
#ifndef _MYPICS_RESOURCE_H
#define _MYPICS_RESOURCE_H

// Strings
#define IDS_DESCRIPTION                 1       // the name of the screen saver
#define IDS_NO_PICTURES                 2       // "There were no pictures found in "%s""
#define IDS_MINUTES_SECONDS             2000
#define IDS_MINUTE_SECONDS              2001
#define IDS_MINUTES_SECOND              2002
#define IDS_MINUTE_SECOND               2003
#define IDS_SECONDS                     2004
#define IDS_SECOND                      2005
#define IDS_MINUTES                     2006
#define IDS_MINUTE                      2007
#define IDS_BROWSE_TITLE                2008
#define IDS_SCREEN_PERCENT              2009

// Icon, and the 1x1 bitmap CScopedDC selects to find the bitmap of a DC
#define IDI_MYPICS                      2005
#define IDB_PLACEHOLDER                 2009

// Settings dialog (DLG_SCRNSAVECONFIGURE of scrnsave.h)
#define IDD_SETTINGS                    2003

#define IDC_CHANGE_INTERVAL             1003    // trackbar, seconds
#define IDC_DIRECTORY                   1004
#define IDC_BROWSE                      1005
#define IDC_CHANGE_INTERVAL_TEXT        1006
#define IDC_SHOW_FILE_NAMES             1008
#define IDC_USE_TRANSITIONS             1009
#define IDC_SCREEN_PERCENT              1010    // trackbar
#define IDC_SCREEN_PERCENT_TEXT         1011
#define IDC_ALLOW_KEYBOARD              1012
#define IDC_STRETCH_SMALL               1013
#define IDC_MORE_OFTEN                  1018
#define IDC_LESS_OFTEN                  1019
#define IDC_SMALLER                     1020
#define IDC_LARGER                      1021
#define IDC_MYPICS_ICON                 1022
#define IDC_TITLE                       1023
#define IDC_CHANGE_INTERVAL_LABEL       1024
#define IDC_SCREEN_PERCENT_LABEL        1025
#define IDC_SEPARATOR                   1026

// Help topics (camera.hlp; the IDs from 20000 on are in windows.hlp)
#define IDH_MYPICS_CHANGE_INTERVAL      1060
#define IDH_MYPICS_SCREEN_PERCENT       1061
#define IDH_MYPICS_DIRECTORY            1062
#define IDH_MYPICS_STRETCH_SMALL        1063
#define IDH_MYPICS_SHOW_FILE_NAMES      1064
#define IDH_MYPICS_USE_TRANSITIONS      1065
#define IDH_MYPICS_ALLOW_KEYBOARD       1066
#define IDH_MYPICS_BROWSE               1067
#define IDH_OK                          28443
#define IDH_CANCEL                      28444

#endif // _MYPICS_RESOURCE_H
