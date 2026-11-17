//-----------------------------------------------------------------------------
// File: ssbranded.h
//
// Desc: The Windows Energy screen saver (ssBranded.scr): the Windows logo,
//       the Microsoft flare, and the "energy" flare with its pills over
//       auroras, connected by wipes - a show that repeats until the screen
//       saver ends.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\ssbranded.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_SSBRANDED_H
#define _BRANDED_SSBRANDED_H

#include "d3dsaver9.h"
#include "ssview.h"

#define SSBRANDED_VIEWS     2       // devices the screen saver keeps objects for
#define SSBRANDED_WIPES     3       // the wipes between the scenes

class CSSBranded : public CD3DScreensaver
{
public:
                    CSSBranded();

    virtual VOID    DoConfig();

protected:
    virtual HRESULT FrameMove();
    virtual HRESULT Render();
    virtual HRESULT FinalCleanup();
    virtual VOID    SetDevice( UINT iDevice );
    virtual HRESULT InitDeviceObjects();
    virtual HRESULT RestoreDeviceObjects();
    virtual HRESULT InvalidateDeviceObjects();
    virtual HRESULT DeleteDeviceObjects();
    virtual HRESULT ConfirmDevice( D3DCAPS9* pCaps, DWORD dwBehavior, D3DFORMAT fmtBackBuffer );

protected:
    bool            m_bDeviceObjectsRestored;
    CSSView         m_aViews[SSBRANDED_VIEWS];

    static CROSSWIPEPARMS s_asCWParms[SSBRANDED_WIPES];
};

#if defined(_WIN64) && defined(UNICODE)
static_assert( sizeof(CSSBranded) == 0x5200, "CSSBranded layout" );
#endif

#endif // _BRANDED_SSBRANDED_H
