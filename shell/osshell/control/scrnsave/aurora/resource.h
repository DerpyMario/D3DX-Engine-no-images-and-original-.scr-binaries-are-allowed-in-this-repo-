//-----------------------------------------------------------------------------
// File: resource.h
//
// Desc: Resource identifiers of the Aurora screen saver.  The numeric values
//       come from Aurora.scr / Aurora.scr.mui; the symbolic names of the
//       Aurora-specific ones are reconstructions.
//-----------------------------------------------------------------------------
#ifndef _AURORA_RESOURCE_H
#define _AURORA_RESOURCE_H

#include "d3dsaver9res.h"       // IDS_DESCRIPTION, IDI_MAIN_ICON, display-settings dialogs, error strings

// "BMP" resource (a Targa file) that is mapped onto the arcs
#define IDR_ARC_TEXTURE                 102

// Strings of the "this screen saver has no options" task dialog
#define IDS_CONFIG_TITLE                300
#define IDS_CONFIG_NOOPTIONS            301
#define IDS_CONFIG_NOOPTIONS_TEXT       302

// Settings dialog.  The template still ships in the .mui file, but the code
// that drove it was removed before Vista was released (see auroraconfig.cpp).
#define IDD_AURORA_SETTINGS             105
#define IDC_SCREENSETTINGS              1002
#define IDC_SPANMULTIMON                1020
#define IDC_NUMLAYERS_BOX               1021
#define IDC_BRIGHTNESS_BOX              1022
#define IDC_SPEED_BOX                   1023
#define IDC_AMPLITUDE_BOX               1024
#define IDC_NUMLAYERS                   1030
#define IDC_BRIGHTNESS                  1031
#define IDC_PRESET1                     1033
#define IDC_PRESET2                     1034
#define IDC_PRESET3                     1035
#define IDC_SPEED                       1036
#define IDC_PRESET4                     1037
#define IDC_AMPLITUDE                   1038

#endif // _AURORA_RESOURCE_H
