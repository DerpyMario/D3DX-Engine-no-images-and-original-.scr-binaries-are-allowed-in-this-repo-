//-----------------------------------------------------------------------------
// File: linearanim.h
//
// Desc: CLinearAnim, a key frame animation with linear interpolation of the
//       position and spherical interpolation of the rotation.  Reads RotPos
//       and XForm keys:
//
//           RotPos:  <frame> <qw> <qx> <qy> <qz> <x> <y> <z>
//           XForm:   <frame> <16 matrix elements, row by row>
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\linearanim.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_LINEARANIM_H
#define _BRANDED_LINEARANIM_H

#include "animation.h"

class CLinearAnim : public CAnimation
{
protected:
    virtual HRESULT LoadKey( ANIM_KEY* pKey, int cchLine, char* pszLine, size_t* pcchLine );
    virtual void    DoTween( ANIM_KEY* pKey0, ANIM_KEY* pKey1, FLOAT fLerp, D3DXQUATERNION& qRot, D3DXVECTOR3& vPos );
};

#endif // _BRANDED_LINEARANIM_H
