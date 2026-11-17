//-----------------------------------------------------------------------------
// File: aurora.h
//
// Desc: CAurora - one band of light of the flare scene's background: a long
//       strip of quads, waved by the vertex shader (three sine waves) and
//       with a texture that scrolls along it.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\aurora.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_AURORA_H
#define _BRANDED_AURORA_H

#include "animmesh.h"

struct AURORAPARMS
{
    FLOAT           fAmp0;          // the three waves (g_fAmpN, g_fFreqN, g_fPhaseN = time * fSpeedN)
    FLOAT           fFreq0;
    FLOAT           fSpeed0;
    FLOAT           fAmp1;
    FLOAT           fFreq1;
    FLOAT           fSpeed1;
    FLOAT           fAmp2;
    FLOAT           fFreq2;
    FLOAT           fSpeed2;
    FLOAT           fScroll;        // texture units per second
    FLOAT           fWrap;          // the texture coordinates are kept within +-2 * fWrap
    FLOAT           fWidth;         // g_fWid
    FLOAT           fHeight;        // g_fHgt
    FLOAT           fYaw;           // orientation, in degrees
    FLOAT           fPitch;
    FLOAT           fRoll;
    D3DXVECTOR3     vPos;
    UINT            cSegments;      // quads along the strip
    DWORD           idTexture;
};

class CAurora : public CAnimMesh
{
public:
                    CAurora();
    virtual         ~CAurora();

    virtual void    OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent );
    virtual void    OnFrameRender( FLOAT fTime, FLOAT fElapsedTime );
    virtual HRESULT OnCreateDevice();

private:
    virtual void    FinalCleanup();
    HRESULT         LoadMesh();

public:
    AURORAPARMS     m_AuroraParms;
    FLOAT           m_fStartTime;
    UINT            m_cVertices;
    UINT            m_cIndices;
    void*           m_pReserved;    // never used
};

#if defined(_WIN64)
static_assert( sizeof(AURORAPARMS) == 0x54, "AURORAPARMS layout" );
static_assert( sizeof(CAurora) == 0x178, "CAurora layout" );
#endif

#endif // _BRANDED_AURORA_H
