//-----------------------------------------------------------------------------
// File: resource.h
//
// Desc: Resource identifiers of the 3D Text screen saver (sstext3d.scr) of
//       Windows XP / Server 2003.  The numeric values are those of the
//       original binary; the identifiers shared with the D3DSaver framework
//       are in ..\d3dsaver\d3dsaverres.h.
//-----------------------------------------------------------------------------
#ifndef _TEXT3D_RESOURCE_H
#define _TEXT3D_RESOURCE_H

#include "../d3dsaver/d3dsaverres.h"

// "JPG" resources: the pictures that are used unless the user picks own ones
#define IDR_TEXTURE                     102
#define IDR_ENVIRONMENT                 103

// Settings dialog
#define IDD_SETTINGS                    105
#define IDC_SCREENSETTINGS              1002
#define IDC_RADIO_TIME                  1004
#define IDC_RADIO_TEXT                  1005
#define IDC_RADIO_COLOR                 1007
#define IDC_RADIO_TEXTURE               1008
#define IDC_RADIO_REFLECTION            1009
#define IDC_SPECULAR                    1012
#define IDC_ROTATION_SPEED              1013
#define IDC_ROTATION_STYLE              1014
#define IDC_RESOLUTION                  1016
#define IDC_SURFACE_COLOR               1017
#define IDC_VIEWPORTSIZE                1020
#define IDC_BROWSE_TEXTURE              1022
#define IDC_BROWSE_ENVIRONMENT          1024
#define IDC_USE_CUSTOM_TEXTURE          1025
#define IDC_USE_CUSTOM_ENVIRONMENT      1026
#define IDC_USE_CUSTOM_COLOR            1027
#define IDC_CHOOSEFONT                  1028
#define IDC_DISPLAYSTRING               1029

// The template of the "Choose Font" dialog (without the size list) uses the
// identifiers of <dlgs.h>: FORMATDLGORD31 (1543), stc1, cmb1, ...

// Strings
#define IDS_DEFAULT_TEXT                1000    // "Microsoft Windows"
#define IDS_ROTATION_NONE               1008    // the entries of the rotation combo box,
#define IDS_ROTATION_SPIN               1009    //   in the order of enum RotType
#define IDS_ROTATION_SEESAW             1010
#define IDS_ROTATION_WOBBLE             1011
#define IDS_ROTATION_TUMBLE             1012
#define IDS_TEXTURE_FILTER              1013
#define IDS_CHOOSE_TEXTURE              1014
#define IDS_CHOOSE_ENVIRONMENT          1015
#define IDS_DEFAULT_FONT                1016

// Where the OpenGL version of this screen saver kept its settings
// (control.ini).  Not all of these are used any more.
#define IDS_INIFILE                     9105    // "control.ini"
#define IDS_SAVERNAME                   9107    // "Screen Saver.3DText"
#define IDS_OPTIONS                     9108
#define IDS_TYPE                        9109
#define IDS_TEXTURE                     9110
#define IDS_TEXTURE_FILE_OFFSET         9111
#define IDS_SIZE                        9112
#define IDS_TESSELATION                 9113
#define IDS_SCREENSAVERTITLE            9114
#define IDS_DEFFONT                     9115
#define IDS_DEFCHARSET                  9116
#define IDS_DEMOTYPE                    9117
#define IDS_SURFSTYLE                   9118
#define IDS_FONT                        9119
#define IDS_FONT_ATTRIBUTES             9120
#define IDS_CHARSET                     9121
#define IDS_TEXT                        9122
#define IDS_SPEED                       9123
#define IDS_ROTSTYLE                    9124

#endif // _TEXT3D_RESOURCE_H
