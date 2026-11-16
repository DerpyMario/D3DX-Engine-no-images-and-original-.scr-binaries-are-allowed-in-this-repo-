//-----------------------------------------------------------------------------
// File: resource.h
//
// Desc: Resource identifiers of the Bubbles screen saver.  The numeric values
//       come from Bubbles.scr / Bubbles.scr.mui; the symbolic names of the
//       Bubbles-specific ones are reconstructions.
//-----------------------------------------------------------------------------
#ifndef _BUBBLES_RESOURCE_H
#define _BUBBLES_RESOURCE_H

#include "d3dsaver9res.h"       // IDS_DESCRIPTION, IDI_MAIN_ICON, display-settings dialogs, error strings

// "BMP" resources (Targa files)
#define IDR_SHADOW                      117     // soft drop shadow
#define IDR_SPHERE_LIT                  118     // shaded sphere - shipped, but never loaded
#define IDR_SPHERE_SOLID                119     // shaded sphere of the "solid" material
#define IDR_SPHERE_HIGHLIGHT            120     // highlights that are added on top of either material
#define IDR_SPHERE_GLASS                121     // translucent rim of the "glass" material

// Strings of the "this screen saver has no options" task dialog
#define IDS_CONFIG_TITLE                300
#define IDS_CONFIG_NOOPTIONS            301
#define IDS_CONFIG_NOOPTIONS_TEXT       302

// Settings dialog.  The template still ships in the .mui file, but the code
// that drove it was removed before Vista was released (see bubblesconfig.cpp).
#define IDD_BUBBLES_SETTINGS            105
#define IDC_SCREENSETTINGS              1002
#define IDC_SHOWBUBBLES                 1020
#define IDC_SHOWSHADOWS                 1021
#define IDC_MATERIAL_SOLID              1022
#define IDC_MATERIAL_GLASS              1023
#define IDC_SPANMULTIMON                1024
#define IDC_NUMSPHERES_BOX              1025
#define IDC_RADIUS_BOX                  1026
#define IDC_TURBULENCEOCTAVES_BOX       1027
#define IDC_TURBULENCESPEED_BOX         1028
#define IDC_TURBULENCEFORCE_BOX         1029
#define IDC_NUMSPHERES                  1030
#define IDC_RADIUS                      1031
#define IDC_PRESET1                     1033
#define IDC_PRESET2                     1034
#define IDC_PRESET3                     1035
#define IDC_TURBULENCEOCTAVES           1036
#define IDC_TURBULENCESPEED             1037
#define IDC_TURBULENCEFORCE             1038

#endif // _BUBBLES_RESOURCE_H
