//-----------------------------------------------------------------------------
// File: resource.h
//
// Desc: Resource identifiers of the 3D Pipes screen saver (sspipes.scr).  The
//       values are those of the original binary; the identifiers shared with
//       the D3DSaver framework are in ..\d3dsaver\d3dsaverres.h.
//-----------------------------------------------------------------------------
#ifndef _PIPES_RESOURCE_H
#define _PIPES_RESOURCE_H

#include "../d3dsaver/d3dsaverres.h"

#define IDD_CONFIGURE                   106
#define IDB_DEFTEX                      107     // default texture (BITMAP)

// Configuration dialog
#define IDC_RADIO_SINGLE_PIPE           3000
#define IDC_RADIO_MULTIPLE_PIPES        3001
#define IDC_STATIC_JOINT_TYPE           3005
#define IDC_RADIO_SOLID                 3006
#define IDC_RADIO_TEX                   3007
#define IDC_COMBO_JOINT_TYPE            3008
#define IDC_BUTTON_TEXTURE              3012
#define IDC_SCREENSETTINGS              3013
#define IDC_SPEED_SLIDER                3014

#define IDS_TEXTUREFILTER               3015
#define IDS_TEXTUREDIALOGTITLE          3016
#define IDS_SAVERNAME                   3017

// Joint types, in the order of JOINT_ELBOW .. JOINT_CYCLE
#define IDS_JOINT_ELBOW                 4001
#define IDS_JOINT_BALL                  4002
#define IDS_JOINT_MIXED                 4003
#define IDS_JOINT_CYCLE                 4004

// Settings of the OpenGL version (control.ini)
#define IDS_JOINT_TYPE                  9000
#define IDS_SURFSTYLE                   9001
#define IDS_TEXQUAL                     9002
#define IDS_MULTIPIPES                  9003
#define IDS_SECTION                     9004
#define IDS_INIFILE                     9005
#define IDS_TEXTURE                     9006

#endif // _PIPES_RESOURCE_H
