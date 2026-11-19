//-----------------------------------------------------------------------------
// File: fstate.cpp
//
// Desc: FLEX_STATE: the scheme of the flexible pipes of a frame.
//
//       Reconstructed from fstate.obj:
//           d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\fstate.obj
//-----------------------------------------------------------------------------
#include "stdafx.h"

// Number of pipes of a frame
#define TURNOMANIA_PIPE_COUNT       10
#define NORMAL_PIPE_COUNT           5
#define NORMAL_TEX_PIPE_COUNT       3




//-----------------------------------------------------------------------------
// Name: FLEX_STATE()
// Desc:
//-----------------------------------------------------------------------------
FLEX_STATE::FLEX_STATE( STATE* pState )
{
    m_pMainState = pState;
    Reset();
}




//-----------------------------------------------------------------------------
// Name: Reset()
// Desc: Chooses the scheme of the next frame
//-----------------------------------------------------------------------------
void FLEX_STATE::Reset()
{
    if( CPipesScreensaver::iRand( 2 ) )
        m_scheme = SC_EXTRUDED_XC;
    else
        m_scheme = SC_TURNOMANIA;
}




//-----------------------------------------------------------------------------
// Name: OKToUseChase()
// Desc: Turning pipes do not chase
//-----------------------------------------------------------------------------
BOOL FLEX_STATE::OKToUseChase()
{
    return m_scheme != SC_TURNOMANIA;
}




//-----------------------------------------------------------------------------
// Name: NewPipe()
// Desc: A new pipe of the scheme
//-----------------------------------------------------------------------------
PIPE* FLEX_STATE::NewPipe( STATE* pState )
{
    if( m_scheme == SC_TURNOMANIA )
        return new TURNING_FLEX_PIPE( pState );
    else
        return new REGULAR_FLEX_PIPE( pState );
}




//-----------------------------------------------------------------------------
// Name: GetMaxPipesPerFrame()
// Desc:
//-----------------------------------------------------------------------------
int FLEX_STATE::GetMaxPipesPerFrame()
{
    if( m_scheme == SC_TURNOMANIA )
        return TURNOMANIA_PIPE_COUNT;
    else
        return m_pMainState->m_bUseTexture ? NORMAL_TEX_PIPE_COUNT : NORMAL_PIPE_COUNT;
}
