//-----------------------------------------------------------------------------
// File: camera.h
//
// Desc: CCamera - view and projection of a scene, optionally driven by an
//       animation.  SetView() pushes the camera's matrices onto the global
//       matrix stacks, RestoreView() pops them.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\camera.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_CAMERA_H
#define _BRANDED_CAMERA_H

#include "vars.h"

class CAnimation;

struct CAMPARMS
{
    FLOAT           fFov;           // vertical, in degrees
    D3DXVECTOR3     vEye;
    D3DXVECTOR3     vAt;
    D3DXVECTOR3     vUp;
};

class CCamera
{
public:
                    CCamera();
                    ~CCamera();

    void            Setup( CAMPARMS* pParms );
    void            OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent );
    void            CreateProjection();
    void            SetView();
    void            RestoreView();

public:
    CAnimation*     m_pAnim;        // moves the camera (its matrix: right, up, direction, position)
    D3DXMATRIX      m_mAnim;
    D3DXMATRIX      m_mProj;
    FLOAT           m_fFov;
    D3DXVECTOR3     m_vEye;
    D3DXVECTOR3     m_vAt;
    D3DXVECTOR3     m_vUp;
    D3DXMATRIX      m_mView;
};

#if defined(_WIN64)
static_assert( sizeof(CAMPARMS) == 0x28, "CAMPARMS layout" );
static_assert( sizeof(CCamera) == 0xf0, "CCamera layout" );
#endif

#endif // _BRANDED_CAMERA_H
