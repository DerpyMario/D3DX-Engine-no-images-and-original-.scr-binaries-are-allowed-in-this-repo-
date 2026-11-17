//-----------------------------------------------------------------------------
// File: vars.cpp
//
// Desc: The global variables of the Windows Energy screen saver (see
//       vars.h).
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\vars.obj
//-----------------------------------------------------------------------------
#include "vars.h"
#include "animation.h"

SSState             g_eState;
SSMsg               g_eMsg;
CSSView*            g_pView;
ID3DXMatrixStack*   g_pViewStack;
ID3DXMatrixStack*   g_pProjStack;
D3DXCOLOR           g_cTargetColor;

D3DXMATRIXA16       g_mIdent( 1.0f, 0.0f, 0.0f, 0.0f,
                              0.0f, 1.0f, 0.0f, 0.0f,
                              0.0f, 0.0f, 1.0f, 0.0f,
                              0.0f, 0.0f, 0.0f, 1.0f );

// No rotation, no translation.  (The dynamic initializer of the original
// also stores an uninitialized float into afReserved[6], which is never
// read.)
ANIM_KEY            g_sDefaultAnimKey = { 0.0f, D3DXQUATERNION( 0.0f, 0.0f, 0.0f, 1.0f ),
                                          D3DXVECTOR3( 0.0f, 0.0f, 0.0f ) };

// debug output buffer (unused in the retail build)
char                sMsg[256];
