//-----------------------------------------------------------------------------
// File: toricanim.h
//
// Desc: CToricAnim, a procedural camera path: the sum of two circles (one in
//       the x-z plane, one in the x-y plane) around a centre, always looking
//       at a fixed point.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\toricanim.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_TORICANIM_H
#define _BRANDED_TORICANIM_H

#include "animation.h"

struct TORICPARMS
{
    D3DXVECTOR3     vCenter;
    D3DXVECTOR3     vLookAt;
    D3DXVECTOR3     vUp;
    FLOAT           fFreq1;         // revolutions per second of the x-z circle
    FLOAT           fPhase1;        // in revolutions
    FLOAT           fFreq2;         // ... and of the x-y circle
    FLOAT           fPhase2;
    FLOAT           fRadius1X;
    FLOAT           fRadius1Z;
    FLOAT           fRadius2X;
    FLOAT           fRadius2Y;
};

class CToricAnim : public CAnimation
{
public:
                    CToricAnim();

    virtual FLOAT   GetLength();
    virtual void    OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mOut );

public:
    TORICPARMS      m_Parms;
};

#if defined(_WIN64)
static_assert( sizeof(TORICPARMS) == 0x44, "TORICPARMS layout" );
static_assert( sizeof(CToricAnim) == 0xc0, "CToricAnim layout" );
#endif

#endif // _BRANDED_TORICANIM_H
