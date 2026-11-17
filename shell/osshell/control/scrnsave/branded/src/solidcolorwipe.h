//-----------------------------------------------------------------------------
// File: solidcolorwipe.h
//
// Desc: CSolidColorWipe - the slowly changing background colour of the flare
//       scene: every few seconds a wipe (with a randomly chosen wipe map)
//       runs from the current colour to the next one of s_acTargetColors.
//       The colour that is being blended to is also published in
//       g_cTargetColor, for the post processing (PostProc.fx).
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\solidcolorwipe.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_SOLIDCOLORWIPE_H
#define _BRANDED_SOLIDCOLORWIPE_H

#include "grapobj.h"
#include "wipe.h"

struct SCWIPEPARMS
{
    FLOAT   fStart;         // first wipe: seconds after the first frame
    FLOAT   fInterval;      // from the start of this wipe to the start of the next one
    FLOAT   fDuration;      // of the wipe (and of the colour blend)
    FLOAT   fWidth;         // of the soft edge
    DWORD   idWipeMap;      // Targa resource
};

#define SCWIPE_COUNT            5
#define SCWIPE_TARGET_COLORS    7
#define SCWIPE_COLOR_SCALE      0.75f   // the colours are darkened by this

class CSolidColorWipe : public CGrapObj
{
public:
    enum ColorOrder
    {
        ORDER_RANDOM = 0,
        ORDER_CYCLE,                    // (never reaches the last colour)
        ORDER_PINGPONG,
    };

                    CSolidColorWipe();
    virtual         ~CSolidColorWipe();

    virtual HRESULT Init();
    virtual void    OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent );
    virtual void    OnFrameRender( FLOAT fTime, FLOAT fElapsedTime );
    virtual HRESULT OnCreateDevice();
    virtual HRESULT OnResetDevice();
    virtual void    OnLostDevice();
    virtual void    OnDestroyDevice();

private:
    virtual void    FinalCleanup();

public:
    CWipe               m_Wipe;
    IDirect3DTexture9** m_ppWipeMaps;       // [m_cWipes]
    ColorOrder          m_eColorOrder;
    bool                m_bForward;         // ORDER_PINGPONG
    bool                m_bFirstFrame;
    FLOAT               m_fWipeStart;       // of the current wipe
    FLOAT               m_fNextWipe;
    UINT                m_cWipes;
    UINT                m_iWipe;
    UINT                m_cColors;
    UINT                m_iColor;
    D3DXCOLOR           m_cPrevColor;
    D3DXCOLOR           m_cNextColor;

private:
    static SCWIPEPARMS  s_asWipeParms[SCWIPE_COUNT];
    static DWORD        s_acTargetColors[SCWIPE_TARGET_COLORS];
};

#if defined(_WIN64)
static_assert( sizeof(SCWIPEPARMS) == 0x14, "SCWIPEPARMS layout" );
static_assert( sizeof(CSolidColorWipe) == 0x1b8, "CSolidColorWipe layout" );
#endif

#endif // _BRANDED_SOLIDCOLORWIPE_H
