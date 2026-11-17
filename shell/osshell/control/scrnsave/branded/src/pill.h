//-----------------------------------------------------------------------------
// File: pill.h
//
// Desc: CPill - one of the glowing "pills" that fly through the flare scene.
//       After its animation has run it waits for a random time and starts
//       again.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\pill.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_PILL_H
#define _BRANDED_PILL_H

#include "animmesh.h"

struct PILLPARMS
{
    ANIMMESHPARMS   mesh;
    FLOAT           fDelay;         // seconds before the first flight
    D3DXMATRIX      mLocal;
};

class CPill : public CAnimMesh
{
public:
                    CPill();

    HRESULT         Init( PILLPARMS* pParms );
    void            Setup( PILLPARMS* pParms );

    virtual void    Go( FLOAT fTime );
    virtual void    OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent );
    virtual void    OnFrameRender( FLOAT fTime, FLOAT fElapsedTime );

public:
    FLOAT           m_fWait;        // until the next flight
};

#if defined(_WIN64)
static_assert( sizeof(PILLPARMS) == 0x5c, "PILLPARMS layout" );
static_assert( sizeof(CPill) == 0x118, "CPill layout" );
#endif

#endif // _BRANDED_PILL_H
