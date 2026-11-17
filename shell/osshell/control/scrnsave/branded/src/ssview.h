//-----------------------------------------------------------------------------
// File: ssview.h
//
// Desc: CSSView - everything that belongs to one Direct3D device: the
//       graphic objects and the scenes that show them.  The current view is
//       g_pView (CSSBranded::SetDevice); the objects use it to reach the
//       device.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\ssview.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_SSVIEW_H
#define _BRANDED_SSVIEW_H

#include "vars.h"
#include "logo.h"
#include "msflarebgnd.h"
#include "msflare.h"
#include "solidcolorwipe.h"
#include "bgnd.h"
#include "flare.h"
#include "logoscene.h"
#include "msflarescene.h"
#include "flarescene.h"
#include "crosswipescene.h"

struct RenderUnit;

class CSSView
{
public:
                    CSSView();
                    ~CSSView();

    HRESULT         InitObjects();
    HRESULT         InitDeviceObjects();
    HRESULT         RestoreDeviceObjects();
    HRESULT         InvalidateDeviceObjects();
    HRESULT         DeleteDeviceObjects();

public:
    IDirect3DDevice9*   m_pd3dDevice;
    RenderUnit*         m_pRenderUnit;
    D3DSURFACE_DESC     m_d3dsdBackBuffer;
    bool                m_bInit;                // InitObjects() was called

    // the scenes, in the order they are shown
    CScene*             m_pLogoScene;
    CCrossWipeScene*    m_pLogoToMSFlare;
    CScene*             m_pMSFlareScene;
    CCrossWipeScene*    m_pMSFlareToFlare;
    CScene*             m_pFlareScene;
    CCrossWipeScene*    m_pFlareToLogo;

    // the graphic objects
    CLogo               m_Logo;
    CMSFlareBgnd        m_MSFlareBgnd;
    CMSFlare            m_MSFlare;
    CSolidColorWipe     m_SolidColorWipe;
    CBgnd               m_Bgnd;
    CFlare              m_Flare;
};

#if defined(_WIN64)
static_assert( sizeof(CSSView) == 0x1460, "CSSView layout" );
#endif

#endif // _BRANDED_SSVIEW_H
