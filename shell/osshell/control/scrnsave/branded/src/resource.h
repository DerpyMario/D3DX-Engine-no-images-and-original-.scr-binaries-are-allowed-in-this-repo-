//-----------------------------------------------------------------------------
// File: resource.h
//
// Desc: Resource identifiers of the Windows Energy screen saver
//       (ssBranded.scr).  The numeric values come from ssBranded.scr and its
//       .mui file; the symbolic names are reconstructions (the effect names
//       are those in the headers of the effect files).
//
//       All the media are RT_RCDATA resources: effects (.fx) and meshes (.x)
//       are text files, the key frame animations a simple text format (see
//       CAnimation::Load), and the pictures Targa files.
//-----------------------------------------------------------------------------
#ifndef _BRANDED_RESOURCE_H
#define _BRANDED_RESOURCE_H

#include "d3dsaver9res.h"       // IDS_DESCRIPTION, error strings

// Strings of the "this screen saver has no options" task dialog
#define IDS_CONFIG_TITLE                300
#define IDS_CONFIG_NOOPTIONS            301
#define IDS_CONFIG_NOOPTIONS_TEXT       302

// meshes
#define IDR_X_QUAD                      10000   // quad.x: full screen quad / sprite
#define IDR_X_FLARE                     10129   // flare.x
#define IDR_X_PILL                      10132   // pill.x

// effects
#define IDR_FX_FSBANNER                 10002   // FSBanner.fx
#define IDR_FX_LOGOGLOW                 10053   // LogoGlow.fx
#define IDR_FX_WIPE                     10124   // Wipe.fx
#define IDR_FX_AURORA                   10128   // Aurora.fx
#define IDR_FX_FLARE                    10131   // Flare.fx
#define IDR_FX_PILL                     10134   // Pill.fx
#define IDR_FX_POSTPROC                 10146   // PostProc.fx
#define IDR_FX_PP_COLORBLOOMH           10147   // PP_ColorBloomH.fx
#define IDR_FX_PP_COLORGBLURV           10148   // PP_ColorGBlurV.fx
#define IDR_FX_PP_COLORBRIGHTPASS       10149   // PP_ColorBrightPass.fx (loaded, never used)
#define IDR_FX_PP_COLORDOWNFILTER4      10150   // PP_ColorDownFilter4.fx
#define IDR_FX_PP_COLORUPFILTER         10151   // PP_ColorUpFilter.fx
#define IDR_FX_PP_COLORCOMBINE4         10152   // PP_ColorCombine4.fx

// key frame animations
#define IDR_ANIM_LOGO                   10054
#define IDR_ANIM_MSFLARE                10118
#define IDR_ANIM_PILL1                  10135
#define IDR_ANIM_PILL2_00               10136
#define IDR_ANIM_PILL2_01               10137
#define IDR_ANIM_PILL2_02               10138
#define IDR_ANIM_PILL2_03               10139
#define IDR_ANIM_PILL2_04               10140
#define IDR_ANIM_PILL2_05               10141
#define IDR_ANIM_PILL2_06               10142
#define IDR_ANIM_PILL2_07               10143
#define IDR_ANIM_PILL2_08               10144
#define IDR_ANIM_PILL2_09               10145
#define IDR_ANIM_XFORM                  10154   // (not used)

// pictures (Targa)
#define IDR_TGA_CROSSWIPE               10001   // wipe map of the scene changes
#define IDR_TGA_LOGO_FIRST              10003   // the frames of the logo
#define IDR_TGA_LOGO_COUNT              50
#define IDR_TGA_MSFLAREBGND             10055
#define IDR_TGA_MSFLARE_FIRST           10056   // the frames of the Microsoft flare
#define IDR_TGA_MSFLARE_COUNT           62
#define IDR_TGA_WIPE0                   10119   // wipe maps of the background colour
#define IDR_TGA_WIPE1                   10120
#define IDR_TGA_WIPE2                   10121
#define IDR_TGA_WIPE3                   10122
#define IDR_TGA_WIPE4                   10123
#define IDR_TGA_AURORA0                 10125   // (not used)
#define IDR_TGA_AURORA1                 10126
#define IDR_TGA_AURORA2                 10127   // (not used)
#define IDR_TGA_AURORA                  IDR_TGA_AURORA1
#define IDR_TGA_FLARE                   10130
#define IDR_TGA_PILL                    10133
#define IDR_TGA_UNUSED                  10153   // (not used)

#endif // _BRANDED_RESOURCE_H
