//-----------------------------------------------------------------------------
// File: grapobj.h
//
// Desc: CGrapObj, the interface of every graphic object and scene of the
//       Windows Energy screen saver.  The member functions follow the device
//       life cycle of the DirectX sample framework (OnCreateDevice,
//       OnResetDevice, OnLostDevice, OnDestroyDevice).
//
//       Times are in seconds (CD3DScreensaver::m_fTime); mParent is the
//       transformation of the object's parent.
//-----------------------------------------------------------------------------
#ifndef _BRANDED_GRAPOBJ_H
#define _BRANDED_GRAPOBJ_H

#include "vars.h"

class CGrapObj
{
public:
    virtual         ~CGrapObj()                                     {}

    virtual HRESULT Init()                                          { return S_OK; }
    virtual void    Reset()                                         {}
    virtual void    Go( FLOAT /*fTime*/ )                           {}
    virtual void    Stop( FLOAT /*fTime*/ )                         {}
    virtual void    OnFrameMove( FLOAT /*fTime*/, FLOAT /*fElapsedTime*/, D3DXMATRIX& /*mParent*/ ) {}
    virtual void    OnFrameRender( FLOAT /*fTime*/, FLOAT /*fElapsedTime*/ ) {}
    virtual HRESULT OnCreateDevice()                                { return S_OK; }
    virtual HRESULT OnResetDevice()                                 { return S_OK; }
    virtual void    OnLostDevice()                                  {}
    virtual void    OnDestroyDevice()                               {}

private:
    virtual void    FinalCleanup()                                  {}
};

#endif // _BRANDED_GRAPOBJ_H
