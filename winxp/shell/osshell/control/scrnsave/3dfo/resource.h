//-----------------------------------------------------------------------------
// File: resource.h
//
// Desc: Resource identifiers of the 3D Flying Objects screen saver
//       (ss3dfo.scr).  The values are the ones of the original binary; the
//       identifiers shared with the D3DSaver framework (dialogs 200/201,
//       strings 2100-2212, icon 101) are in ..\d3dsaver\d3dsaverres.h.
//-----------------------------------------------------------------------------
#ifndef _SS3DFO_RESOURCE_H
#define _SS3DFO_RESOURCE_H

#include "../d3dsaver/d3dsaverres.h"

// Configuration dialog (the DLG_SCRNSAVECONFIGURE id of scrnsave.h)
#define DLG_SCRNSAVECONFIGURE           2003

#define DLG_SETUP_TYPES                 2002    // style combo box
#define DLG_SETUP_SMOOTH                2005
#define DLG_SETUP_CYCLE                 2006
#define DLG_SETUP_TESSEL                2008    // "Resolution" trackbar
#define DLG_SETUP_SIZE                  2009
#define DLG_SETUP_TEXTURE               2010    // "Texture..."
#define IDC_STATIC_TESS                 2011
#define IDC_STATIC_TESS_MIN             2012
#define IDC_STATIC_TESS_MAX             2013
#define IDC_STATIC_SIZE                 2014
#define IDC_STATIC_SIZE_MIN             2015
#define IDC_STATIC_SIZE_MAX             2016
#define IDC_SCREENSETTINGS              2017    // "Display Settings..."

// Pictures
#define IDR_FLAG_JPG                    3000    // default texture of the flag
#define IDR_LOGO_DDS                    3001    // the Windows flag
#define IDR_LOGO_PNG                    3002    // the Windows name

// Style names
#define IDS_LOGO                        1100
#define IDS_EXPLODE                     1101
#define IDS_RIBBON                      1102
#define IDS_2RIBBONS                    1103
#define IDS_SPLASH                      1104
#define IDS_TWIST                       1105
#define IDS_FLAG                        1106

// Settings of the OpenGL version (control.ini)
#define IDS_SAVERNAME                   1107
#define IDS_OPTIONS                     1108
#define IDS_OBJTYPE                     1109
#define IDS_TEXTURE                     1110
#define IDS_TEXTURE_FILE_OFFSET         1111
#define IDS_SIZE                        1112
#define IDS_TESSELATION                 1113
#define IDS_INIFILE                     1114

// Texture files (strings of the screen saver common library)
#define IDS_WARNING                     9014
#define IDS_ERROR                       9015
#define IDS_TEXTURE_SIZE                9016
#define IDS_TEXTURE_INVALID             9017
#define IDS_BITMAP_INVALID              9018
#define IDS_START_FAILED                9019
#define IDS_TEXTUREFILTER               9021
#define IDS_TEXTUREDIALOGTITLE          9022
#define IDS_BMP                         9023
#define IDS_DOTBMP                      9024
#define IDS_STARDOTBMP                  9025
#define IDS_RGB                         9030
#define IDS_DOTRGB                      9031
#define IDS_STARDOTRGB                  9032

#endif // _SS3DFO_RESOURCE_H
