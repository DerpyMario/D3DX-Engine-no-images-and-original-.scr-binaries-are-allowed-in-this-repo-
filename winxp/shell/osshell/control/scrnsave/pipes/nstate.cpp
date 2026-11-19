//-----------------------------------------------------------------------------
// File: nstate.cpp
//
// Desc: NORMAL_STATE: the joint style and the objects (pipes, elbows, balls)
//       the normal pipes are drawn with.
//
//       Reconstructed from nstate.obj:
//           d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\nstate.obj
//-----------------------------------------------------------------------------
#include "stdafx.h"

#define NUM_JOINT_STYLES    3




//-----------------------------------------------------------------------------
// Name: NORMAL_STATE()
// Desc:
//-----------------------------------------------------------------------------
NORMAL_STATE::NORMAL_STATE( STATE* pState )
{
    m_pd3dDevice = pState->m_pd3dDevice;

    // Joint style of the settings
    m_bCycleJointStyles = FALSE;

    switch( pState->m_pConfig->nJointType )
    {
        case JOINT_ELBOW:
            m_jointStyle = ELBOWS;
            break;
        case JOINT_BALL:
            m_jointStyle = BALLS;
            break;
        case JOINT_MIXED:
            m_jointStyle = EITHER;
            break;
        case JOINT_CYCLE:
            m_bCycleJointStyles = TRUE;
            m_jointStyle = EITHER;
            break;
    }

    BuildObjects( pState->m_radius, pState->m_view.m_divSize, pState->m_nSlices,
                  pState->m_bUseTexture, &pState->m_texRep[0] );
}




//-----------------------------------------------------------------------------
// Name: BuildObjects()
// Desc: Builds the objects of the pipes.  With a texture, the s coordinates
//       of the objects match where they meet.
//-----------------------------------------------------------------------------
void NORMAL_STATE::BuildObjects( FLOAT radius, FLOAT divSize, int nSlices,
                                 BOOL bTexture, IPOINT2D* pTexRep )
{
    OBJECT_BUILD_INFO buildInfo;
    int i;

    buildInfo.radius   = radius;
    buildInfo.divSize  = divSize;
    buildInfo.nSlices  = nSlices;
    buildInfo.bTexture = bTexture;
    buildInfo.texRep   = NULL;

    if( bTexture )
    {
        buildInfo.texRep = pTexRep;

        // s coordinates where the short pipes start (end of an elbow)
        FLOAT s_max = (FLOAT) pTexRep->y;
        FLOAT s_trans = s_max * 2.0f * radius / divSize;

        // Pipes
        m_shortPipe = new PIPE_OBJECT( m_pd3dDevice, &buildInfo, divSize - 2*radius,
                                       s_trans, s_max );
        m_longPipe = new PIPE_OBJECT( m_pd3dDevice, &buildInfo, divSize,
                                      0.0f, s_max );

        // Elbows and ball joints
        for( i = 0; i < 4; i ++ )
        {
            m_elbows[i] = new ELBOW_OBJECT( m_pd3dDevice, &buildInfo, i, 0.0f, s_trans );
            m_ballJoints[i] = new BALLJOINT_OBJECT( m_pd3dDevice, &buildInfo, i, 0.0f, s_trans );
        }

        m_bigBall = NULL;

        // End cap: centered on the node, its texture runs from the back of
        // the ball to the start of the short pipe
        int texRepX = pTexRep->x;
        FLOAT s_start = - texRepX * (ROOT_TWO - 1.0f) * radius / divSize;
        FLOAT s_end = texRepX * (2.0f + (ROOT_TWO - 1.0f)) * radius / divSize;

        // Shift the s coordinates so that none is negative
        FLOAT comp_s = 1.0f - (int) s_start;
        s_start += comp_s;
        s_end += comp_s;

        m_ballCap = new SPHERE_OBJECT( m_pd3dDevice, &buildInfo, ROOT_TWO*radius,
                                       s_start, s_end );
    }
    else
    {
        // Pipes
        m_shortPipe = new PIPE_OBJECT( m_pd3dDevice, &buildInfo, divSize - 2*radius );
        m_longPipe = new PIPE_OBJECT( m_pd3dDevice, &buildInfo, divSize );

        // Elbows
        for( i = 0; i < 4; i ++ )
        {
            m_elbows[i] = new ELBOW_OBJECT( m_pd3dDevice, &buildInfo, i );
            m_ballJoints[i] = NULL;
        }

        // Without a texture there is only one ball joint.  It is a little
        // larger than the pipes so that no edge of them sticks out of it.
        m_bigBall = new SPHERE_OBJECT( m_pd3dDevice, &buildInfo,
                                       ROOT_TWO*radius / ((FLOAT) cos( PI/nSlices )) );

        // End cap
        m_ballCap = new SPHERE_OBJECT( m_pd3dDevice, &buildInfo, ROOT_TWO*radius );
    }
}




//-----------------------------------------------------------------------------
// Name: Reset()
// Desc: Goes to the next joint style when cycling them
//-----------------------------------------------------------------------------
void NORMAL_STATE::Reset()
{
    if( m_bCycleJointStyles )
    {
        if( ++m_jointStyle >= NUM_JOINT_STYLES )
            m_jointStyle = 0;
    }
}




//-----------------------------------------------------------------------------
// Name: ChooseJointType()
// Desc:
//-----------------------------------------------------------------------------
int NORMAL_STATE::ChooseJointType()
{
    switch( m_jointStyle )
    {
        case ELBOWS:
            return ELBOW_JOINT;
        case BALLS:
            return BALL_JOINT;
        case EITHER:
        default:
            // a ball now and then
            return (CPipesScreensaver::iRand( 3 ) == 0) ? BALL_JOINT : ELBOW_JOINT;
    }
}




//-----------------------------------------------------------------------------
// Name: ~NORMAL_STATE()
// Desc:
//-----------------------------------------------------------------------------
NORMAL_STATE::~NORMAL_STATE()
{
    SAFE_DELETE( m_shortPipe );
    SAFE_DELETE( m_longPipe );
    SAFE_DELETE( m_ballCap );
    SAFE_DELETE( m_bigBall );

    for( int i = 0; i < 4; i ++ )
    {
        SAFE_DELETE( m_elbows[i] );
        SAFE_DELETE( m_ballJoints[i] );
    }
}
