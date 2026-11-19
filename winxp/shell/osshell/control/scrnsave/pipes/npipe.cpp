//-----------------------------------------------------------------------------
// File: npipe.cpp
//
// Desc: NORMAL_PIPE: the classic pipes, made of cylinders joined by elbows
//       or balls.
//
//       A cylinder has a notch (a seam: where the texture wraps).  The
//       tables below keep track of the direction the notch points to, so
//       that the elbows and the pipes after them line up.
//
//       Reconstructed from npipe.obj:
//           d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\npipe.obj
//-----------------------------------------------------------------------------
#include "stdafx.h"

#define iXX     -1          // invalid entry
#define fXX     -0.01f      // invalid entry

#define DEG_TO_RAD( a )     ( (a) * PI / 180.0f )


// The direction of the notch after align_plusz( newDir )
static int defCylNotch[NUM_DIRS] =
        { PLUS_Y, PLUS_Y, MINUS_Z, PLUS_Z, PLUS_Y, PLUS_Y };

// The notch directions of the four elbows, per [oldDir][newDir]
static int elbows[NUM_DIRS][NUM_DIRS][4] =
{
    // old PLUS_X
    { { iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX     },
      { PLUS_Y , MINUS_Z, MINUS_Y, PLUS_Z  },
      { MINUS_Y, PLUS_Z , PLUS_Y , MINUS_Z },
      { PLUS_Z , PLUS_Y , MINUS_Z, MINUS_Y },
      { MINUS_Z, MINUS_Y, PLUS_Z , PLUS_Y  } },
    // old MINUS_X
    { { iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX     },
      { PLUS_Y , PLUS_Z , MINUS_Y, MINUS_Z },
      { MINUS_Y, MINUS_Z, PLUS_Y , PLUS_Z  },
      { PLUS_Z , MINUS_Y, MINUS_Z, PLUS_Y  },
      { MINUS_Z, PLUS_Y , PLUS_Z , MINUS_Y } },
    // old PLUS_Y
    { { PLUS_X , PLUS_Z , MINUS_X, MINUS_Z },
      { MINUS_X, MINUS_Z, PLUS_X , PLUS_Z  },
      { iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX     },
      { PLUS_Z , MINUS_X, MINUS_Z, PLUS_X  },
      { MINUS_Z, PLUS_X , PLUS_Z , MINUS_X } },
    // old MINUS_Y
    { { PLUS_X , MINUS_Z, MINUS_X, PLUS_Z  },
      { MINUS_X, PLUS_Z , PLUS_X , MINUS_Z },
      { iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX     },
      { PLUS_Z , PLUS_X , MINUS_Z, MINUS_X },
      { MINUS_Z, MINUS_X, PLUS_Z , PLUS_X  } },
    // old PLUS_Z
    { { PLUS_X , MINUS_Y, MINUS_X, PLUS_Y  },
      { MINUS_X, PLUS_Y , PLUS_X , MINUS_Y },
      { PLUS_Y , PLUS_X , MINUS_Y, MINUS_X },
      { MINUS_Y, MINUS_X, PLUS_Y , PLUS_X  },
      { iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX     } },
    // old MINUS_Z
    { { PLUS_X , PLUS_Y , MINUS_X, MINUS_Y },
      { MINUS_X, MINUS_Y, PLUS_X , PLUS_Y  },
      { PLUS_Y , MINUS_X, MINUS_Y, PLUS_X  },
      { MINUS_Y, PLUS_X , PLUS_Y , MINUS_X },
      { iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX     } },
};

// Rotation about z (degrees) that puts an elbow in place after
// align_plusz( newDir ), per [oldDir][newDir]
static FLOAT alignPlusYRotZ[NUM_DIRS][NUM_DIRS] =
{
    {    0.0f,    0.0f,   90.0f,   90.0f,   90.0f,  -90.0f },
    {    0.0f,    0.0f,  -90.0f,  -90.0f,  -90.0f,   90.0f },
    {  180.0f,  180.0f,    0.0f,    0.0f,  180.0f,  180.0f },
    {    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f },
    {  -90.0f,   90.0f,    0.0f,  180.0f,    0.0f,    0.0f },
    {   90.0f,  -90.0f,  180.0f,    0.0f,    0.0f,    0.0f },
};

// Rotation about z (degrees) that turns the notch of a cylinder from its
// default direction to the wanted one, per [newDir][notchVec]
static FLOAT alignNotchRotZ[NUM_DIRS][NUM_DIRS] =
{
    {     fXX,     fXX,    0.0f,  180.0f,   90.0f,  -90.0f },
    {     fXX,     fXX,    0.0f,  180.0f,  -90.0f,   90.0f },
    {  -90.0f,   90.0f,     fXX,     fXX,  180.0f,    0.0f },
    {  -90.0f,   90.0f,     fXX,     fXX,    0.0f,  180.0f },
    {  -90.0f,   90.0f,    0.0f,  180.0f,     fXX,     fXX },
    {   90.0f,  -90.0f,    0.0f,  180.0f,     fXX,     fXX },
};

// The direction of the notch after a turn, per [oldDir][newDir][notchVec]
int notchTurn[NUM_DIRS][NUM_DIRS][NUM_DIRS] =
{
    // old PLUS_X
    { { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , MINUS_X, PLUS_X , PLUS_Z , MINUS_Z },
      { iXX    , iXX    , PLUS_X , MINUS_X, PLUS_Z , MINUS_Z },
      { iXX    , iXX    , PLUS_Y , MINUS_Y, MINUS_X, PLUS_X  },
      { iXX    , iXX    , PLUS_Y , MINUS_Y, PLUS_X , MINUS_X } },
    // old MINUS_X
    { { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , PLUS_X , MINUS_X, PLUS_Z , MINUS_Z },
      { iXX    , iXX    , MINUS_X, PLUS_X , PLUS_Z , MINUS_Z },
      { iXX    , iXX    , PLUS_Y , MINUS_Y, PLUS_X , MINUS_X },
      { iXX    , iXX    , PLUS_Y , MINUS_Y, MINUS_X, PLUS_X  } },
    // old PLUS_Y
    { { MINUS_Y, PLUS_Y , iXX    , iXX    , PLUS_Z , MINUS_Z },
      { PLUS_Y , MINUS_Y, iXX    , iXX    , PLUS_Z , MINUS_Z },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { PLUS_X , MINUS_X, iXX    , iXX    , MINUS_Y, PLUS_Y  },
      { PLUS_X , MINUS_X, iXX    , iXX    , PLUS_Y , MINUS_Y } },
    // old MINUS_Y
    { { PLUS_Y , MINUS_Y, iXX    , iXX    , PLUS_Z , MINUS_Z },
      { MINUS_Y, PLUS_Y , iXX    , iXX    , PLUS_Z , MINUS_Z },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { PLUS_X , MINUS_X, iXX    , iXX    , PLUS_Y , MINUS_Y },
      { PLUS_X , MINUS_X, iXX    , iXX    , MINUS_Y, PLUS_Y  } },
    // old PLUS_Z
    { { MINUS_Z, PLUS_Z , PLUS_Y , MINUS_Y, iXX    , iXX     },
      { PLUS_Z , MINUS_Z, PLUS_Y , MINUS_Y, iXX    , iXX     },
      { PLUS_X , MINUS_X, MINUS_Z, PLUS_Z , iXX    , iXX     },
      { PLUS_X , MINUS_X, PLUS_Z , MINUS_Z, iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     } },
    // old MINUS_Z
    { { PLUS_Z , MINUS_Z, PLUS_Y , MINUS_Y, iXX    , iXX     },
      { MINUS_Z, PLUS_Z , PLUS_Y , MINUS_Y, iXX    , iXX     },
      { PLUS_X , MINUS_X, PLUS_Z , MINUS_Z, iXX    , iXX     },
      { PLUS_X , MINUS_X, MINUS_Z, PLUS_Z , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     } },
};




//-----------------------------------------------------------------------------
// Name: NORMAL_PIPE()
// Desc:
//-----------------------------------------------------------------------------
NORMAL_PIPE::NORMAL_PIPE( STATE* pState ) : PIPE( pState )
{
    m_type = TYPE_NORMAL;
    m_pNState = pState->m_pNState;

    // Choose the weight of going straight on: now and then a pipe that
    // (almost) never turns
    if( !CPipesScreensaver::iRand( 20 ) )
        m_weightStraight = CPipesScreensaver::iRand2( 25, 100 );
    else
        m_weightStraight = 1 + CPipesScreensaver::iRand( 4 );
}




//-----------------------------------------------------------------------------
// Name: DrawStartCap()
// Desc: Draws the ball (or the textured cap) the pipe starts with, and
//       aligns +z with the first direction
//-----------------------------------------------------------------------------
void NORMAL_PIPE::DrawStartCap( int newDir )
{
    if( m_pState->m_bUseTexture )
    {
        align_plusz( newDir );
        m_pNState->m_ballCap->Draw( m_pWorldMatrixStack->GetTop() );
    }
    else
    {
        // big ball in the default orientation
        m_pNState->m_bigBall->Draw( m_pWorldMatrixStack->GetTop() );
        align_plusz( newDir );
    }
}




//-----------------------------------------------------------------------------
// Name: ChooseElbow()
// Desc: The elbow (0-3) whose notch matches the current notch
//-----------------------------------------------------------------------------
int NORMAL_PIPE::ChooseElbow( int oldDir, int newDir )
{
    for( int i = 0; i < 4; i++ )
    {
        if( elbows[oldDir][newDir][i] == m_notchVec )
            return i;
    }

    return -1;
}




//-----------------------------------------------------------------------------
// Name: align_plusy()
// Desc: Rotates about z to put an elbow in place (+z is aligned with newDir)
//-----------------------------------------------------------------------------
void NORMAL_PIPE::align_plusy( int oldDir, int newDir )
{
    static D3DXVECTOR3 zAxis( 0.0f, 0.0f, 1.0f );
    FLOAT rotz = alignPlusYRotZ[oldDir][newDir];

    if( rotz != 0.0f )
        m_pWorldMatrixStack->RotateAxisLocal( &zAxis, DEG_TO_RAD( rotz ) );
}




//-----------------------------------------------------------------------------
// Name: align_notch()
// Desc: Rotates about z to turn the notch of a cylinder (along newDir)
//       towards the notch direction
//-----------------------------------------------------------------------------
void NORMAL_PIPE::align_notch( int newDir, int notch )
{
    FLOAT rotz = alignNotchRotZ[newDir][notch];
    static D3DXVECTOR3 zAxis( 0.0f, 0.0f, 1.0f );

    if( rotz != 0.0f )
        m_pWorldMatrixStack->RotateAxisLocal( &zAxis, DEG_TO_RAD( rotz ) );
}




//-----------------------------------------------------------------------------
// Name: Start()
// Desc: Starts the pipe at an empty node with a start cap and a short pipe
//-----------------------------------------------------------------------------
void NORMAL_PIPE::Start()
{
    int newDir;

    // Find an empty node to start from
    if( !SetStartPos() )
    {
        m_status = PIPE_OUT_OF_NODES;
        return;
    }

    ChooseMaterial();

    m_pState->m_pd3dDevice->SetTexture( 0, m_pState->m_texInfo[0].pTexture );
    m_pState->m_pd3dDevice->SetMaterial( m_pMat );

    m_pWorldMatrixStack->Push();
    TranslateToCurrentPosition();

    // Any direction is fine for the first node
    m_lastDir = CPipesScreensaver::iRand( NUM_DIRS );

    newDir = ChooseNewDirection();
    if( newDir == DIR_NONE )
    {
        // Nowhere to go from here
        m_status = PIPE_STUCK;
        m_pWorldMatrixStack->Pop();
        return;
    }

    m_status = PIPE_ACTIVE;
    m_notchVec = defCylNotch[newDir];

    DrawStartCap( newDir );

    // Short pipe to the next node
    m_pWorldMatrixStack->TranslateLocal( 0.0f, 0.0f, m_radius );
    align_notch( newDir, m_notchVec );
    m_pNState->m_shortPipe->Draw( m_pWorldMatrixStack->GetTop() );

    m_pWorldMatrixStack->Pop();

    UpdateCurrentPosition( newDir );
    m_lastDir = newDir;
}




//-----------------------------------------------------------------------------
// Name: DrawEndCap()
// Desc: Ends a stuck pipe with a ball (or the textured cap)
//-----------------------------------------------------------------------------
void NORMAL_PIPE::DrawEndCap()
{
    m_pWorldMatrixStack->Push();
    TranslateToCurrentPosition();

    if( m_pState->m_bUseTexture )
    {
        align_plusz( m_lastDir );
        align_notch( m_lastDir, m_notchVec );
        m_pNState->m_ballCap->Draw( m_pWorldMatrixStack->GetTop() );
    }
    else
    {
        m_pNState->m_bigBall->Draw( m_pWorldMatrixStack->GetTop() );
    }

    m_pWorldMatrixStack->Pop();
}




//-----------------------------------------------------------------------------
// Name: DrawJoint()
// Desc: Draws the joint (an elbow or a ball) of a turn from m_lastDir to
//       newDir, and moves to the end of it
//-----------------------------------------------------------------------------
void NORMAL_PIPE::DrawJoint( int newDir )
{
    int jointType = m_pNState->ChooseJointType();
    int iBend;

    switch( jointType )
    {
        case BALL_JOINT:
            if( m_pState->m_bUseTexture )
            {
                // Textured ball joints have the orientations of the elbows
                align_plusz( newDir );
                m_pWorldMatrixStack->Push();
                align_plusy( m_lastDir, newDir );

                // Move forward to the centre of the elbow
                m_pWorldMatrixStack->TranslateLocal( 0.0f, 0.0f, m_radius );

                iBend = ChooseElbow( m_lastDir, newDir );
                m_pNState->m_ballJoints[iBend]->Draw( m_pWorldMatrixStack->GetTop() );
                m_pWorldMatrixStack->Pop();
            }
            else
            {
                // Big ball in the default orientation
                m_pNState->m_bigBall->Draw( m_pWorldMatrixStack->GetTop() );
                align_plusz( newDir );
            }
            break;

        case ELBOW_JOINT:
        default:
            align_plusz( newDir );
            m_pWorldMatrixStack->Push();
            align_plusy( m_lastDir, newDir );

            // Move forward to the centre of the elbow
            m_pWorldMatrixStack->TranslateLocal( 0.0f, 0.0f, m_radius );

            iBend = ChooseElbow( m_lastDir, newDir );
            if( iBend == -1 )
                iBend = 0;
            m_pNState->m_elbows[iBend]->Draw( m_pWorldMatrixStack->GetTop() );
            m_pWorldMatrixStack->Pop();
            break;
    }

    // Move to the end of the joint
    m_pWorldMatrixStack->TranslateLocal( 0.0f, 0.0f, m_radius );

    // Where the notch points to now
    m_notchVec = notchTurn[m_lastDir][newDir][m_notchVec];
}




//-----------------------------------------------------------------------------
// Name: Draw()
// Desc: Draws the pipe to the next node: a long pipe if it goes straight
//       on, else a joint and a short pipe
//-----------------------------------------------------------------------------
void NORMAL_PIPE::Draw()
{
    int newDir;

    m_pState->m_pd3dDevice->SetTexture( 0, m_pState->m_texInfo[0].pTexture );
    m_pState->m_pd3dDevice->SetMaterial( m_pMat );

    newDir = ChooseNewDirection();
    if( newDir == DIR_NONE )
    {
        // The pipe is stuck: end it
        DrawEndCap();
        m_status = PIPE_STUCK;
        return;
    }

    m_pWorldMatrixStack->Push();
    TranslateToCurrentPosition();

    if( newDir == m_lastDir )
    {
        // Straight on: a long pipe from the end of the last one
        align_plusz( newDir );
        align_notch( newDir, m_notchVec );
        m_pWorldMatrixStack->TranslateLocal( 0.0f, 0.0f, -m_radius );
        m_pNState->m_longPipe->Draw( m_pWorldMatrixStack->GetTop() );
    }
    else
    {
        // A turn: a joint, then a short pipe
        DrawJoint( newDir );
        align_notch( newDir, m_notchVec );
        m_pNState->m_shortPipe->Draw( m_pWorldMatrixStack->GetTop() );
    }

    m_pWorldMatrixStack->Pop();

    UpdateCurrentPosition( newDir );
    m_lastDir = newDir;
}
