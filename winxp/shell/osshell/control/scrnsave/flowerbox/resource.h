//-----------------------------------------------------------------------------
// File: resource.h
//
// Desc: Resource identifiers of the 3D FlowerBox screen saver
//       (ssflwbox.scr).  The values are the ones of the original binary; the
//       identifiers shared with the D3DSaver framework (dialogs 200/201,
//       strings 2100-2212, icon 101) are in ..\d3dsaver\d3dsaverres.h.
//-----------------------------------------------------------------------------
#ifndef _FLOWERBOX_RESOURCE_H
#define _FLOWERBOX_RESOURCE_H

#include "../d3dsaver/d3dsaverres.h"

// Configuration dialog (the DLG_SCRNSAVECONFIGURE id of scrnsave.h)
#define DLG_SCRNSAVECONFIGURE           2003

#define DLG_SETUP_SUBDIV                401     // "Complexity" trackbar
#define DLG_SETUP_SIZE                  402     // "Size" trackbar

#define ID_COL_CHECKER                  500     // Coloring: checkerboard
#define ID_COL_PER_SIDE                 501     // Coloring: one color per side
#define ID_COL_SINGLE                   502     // Coloring: one color
#define ID_COL_SMOOTH                   550
#define ID_COL_TRIANGLE                 551     // "Slanted"
#define ID_COL_CYCLE                    552

#define ID_SPIN                         600
#define ID_BLOOM                        601
#define ID_TWO_SIDED                    602

#define ID_GEOM                         650     // Shape combo box
#define IDC_SCREENSETTINGS              651     // "Display Settings..."

// Shape names (the combo box lists IDS_GEOM_CUBE + 0 .. 4)
#define IDS_GEOM_CUBE                   1025
#define IDS_GEOM_TETRA                  1026
#define IDS_GEOM_PYRAMIDS               1027
#define IDS_GEOM_CYLINDER               1028
#define IDS_GEOM_SPRING                 1029

// Settings of the OpenGL version of the screen saver, read once from
// control.ini (the [Screen Saver.3DFlowerBox] section) for upgrades
#define IDS_INIFILE                     1113
#define IDS_SAVERNAME                   1114
#define IDS_CONFIG_TRIANGLE_COLORS      1115    // "Slanted"
#define IDS_CONFIG_CYCLE_COLORS         1116    // "Cycle"
#define IDS_CONFIG_SPIN                 1117    // "Spin"
#define IDS_CONFIG_BLOOM                1118    // "Bloom"
#define IDS_CONFIG_SUBDIV               1119    // "Subdiv"
#define IDS_CONFIG_COLOR_PICK           1120    // "ColorPick"
#define IDS_CONFIG_IMAGE_SIZE           1121    // "ImageSize"
#define IDS_CONFIG_GEOM                 1122    // "Geom"
#define IDS_CONFIG_TWO_SIDED            1123    // "TwoSided"
#define IDS_CONFIG_SMOOTH_COLORS        1124    // "Smooth"

#endif // _FLOWERBOX_RESOURCE_H
