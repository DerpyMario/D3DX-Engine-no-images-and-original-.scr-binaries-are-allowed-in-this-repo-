//-----------------------------------------------------------------------------
// File: msflarebgnd.h
//
// Desc: CMSFlareBgnd - the full screen picture behind the Microsoft flare.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\msflarebgnd.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_MSFLAREBGND_H
#define _BRANDED_MSFLAREBGND_H

#include "animmesh.h"

class CMSFlareBgnd : public CAnimMesh
{
public:
    using CAnimMesh::Init;

    virtual HRESULT Init();
};

#endif // _BRANDED_MSFLAREBGND_H
