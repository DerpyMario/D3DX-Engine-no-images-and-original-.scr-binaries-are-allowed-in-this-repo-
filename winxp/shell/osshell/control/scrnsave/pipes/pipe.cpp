//-----------------------------------------------------------------------------
// File: pipe.cpp
//
// Desc: PIPE: what all the pipes have in common: their position in the node
//       array, how they choose their start position and their directions.
//
//       Reconstructed from pipe.obj:
//           d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\pipe.obj
//-----------------------------------------------------------------------------
#include "stdafx.h"




//-----------------------------------------------------------------------------
// Name: PIPE()
// Desc:
//-----------------------------------------------------------------------------
PIPE::PIPE( STATE* pState )
{
    m_pState            = pState;
    m_pWorldMatrixStack = m_pState->m_pWorldMatrixStack;
    m_radius            = m_pState->m_radius;

    // default methods
    m_chooseDirMethod      = CHOOSE_DIR_RANDOM_WEIGHTED;
    m_chooseStartPosMethod = CHOOSE_STARTPOS_RANDOM;
    m_weightStraight       = 1;
}

PIPE::~PIPE()
{
}




//-----------------------------------------------------------------------------
// Name: ChooseMaterial()
// Desc:
//-----------------------------------------------------------------------------
void PIPE::ChooseMaterial()
{
    if( m_pState->m_bUseTexture )
        m_pMat = RandomTexMaterial();
    else
        m_pMat = RandomTeaMaterial();
}




//-----------------------------------------------------------------------------
// Name: SetChooseDirectionMethod()
// Desc:
//-----------------------------------------------------------------------------
void PIPE::SetChooseDirectionMethod( int method )
{
    m_chooseDirMethod = method;
}




//-----------------------------------------------------------------------------
// Name: GetBestDirsForChase()
// Desc: The directions that lead towards the lead pipe
//-----------------------------------------------------------------------------
int PIPE::GetBestDirsForChase( int* bestDirs )
{
    IPOINT3D* leadPos = &m_pState->m_pLeadPipe->m_curPos;
    IPOINT3D* curPos = &m_curPos;
    int numDirs = 0;

    if( leadPos->x != curPos->x )
    {
        *bestDirs++ = (leadPos->x > curPos->x) ? PLUS_X : MINUS_X;
        numDirs++;
    }
    if( leadPos->y != curPos->y )
    {
        *bestDirs++ = (leadPos->y > curPos->y) ? PLUS_Y : MINUS_Y;
        numDirs++;
    }
    if( leadPos->z != curPos->z )
    {
        *bestDirs++ = (leadPos->z > curPos->z) ? PLUS_Z : MINUS_Z;
        numDirs++;
    }

    return numDirs;
}




//-----------------------------------------------------------------------------
// Name: SetChooseStartPosMethod()
// Desc:
//-----------------------------------------------------------------------------
void PIPE::SetChooseStartPosMethod( int method )
{
    m_chooseStartPosMethod = method;
}




//-----------------------------------------------------------------------------
// Name: SetStartPos()
// Desc: Takes an empty node to start from: a random one, or the closest one
//       to the corner of the grid furthest away from the current position.
//-----------------------------------------------------------------------------
BOOL PIPE::SetStartPos()
{
    NODE_ARRAY* nodes = m_pState->m_nodes;

    switch( m_chooseStartPosMethod )
    {
        case CHOOSE_STARTPOS_FURTHEST:
        {
            IPOINT3D numNodes, newPos;

            nodes->GetNodeCount( &numNodes );

            newPos.x = (m_curPos.x < numNodes.x / 2) ? numNodes.x - 1 : 0;
            newPos.y = (m_curPos.y < numNodes.y / 2) ? numNodes.y - 1 : 0;
            newPos.z = (m_curPos.z < numNodes.z / 2) ? numNodes.z - 1 : 0;

            if( !nodes->TakeClosestEmptyNode( &m_curPos, &newPos ) )
                return FALSE;
            break;
        }

        default:
            if( !nodes->FindRandomEmptyNode( &m_curPos ) )
                return FALSE;
            break;
    }

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: IsStuck()
// Desc:
//-----------------------------------------------------------------------------
BOOL PIPE::IsStuck()
{
    return m_status == PIPE_STUCK;
}




//-----------------------------------------------------------------------------
// Name: TranslateToCurrentPosition()
// Desc: Moves the origin to the current node (the grid is centered on the
//       origin)
//-----------------------------------------------------------------------------
void PIPE::TranslateToCurrentPosition()
{
    IPOINT3D numNodes;
    FLOAT divSize = m_pState->m_view.m_divSize;

    m_pState->m_nodes->GetNodeCount( &numNodes );

    m_pWorldMatrixStack->TranslateLocal( (m_curPos.x - (numNodes.x - 1) / 2.0f) * divSize,
                                         (m_curPos.y - (numNodes.y - 1) / 2.0f) * divSize,
                                         (m_curPos.z - (numNodes.z - 1) / 2.0f) * divSize );
}




//-----------------------------------------------------------------------------
// Name: UpdateCurrentPosition()
// Desc: Moves the current position to the next node in the given direction
//-----------------------------------------------------------------------------
void PIPE::UpdateCurrentPosition( int newDir )
{
    switch( newDir )
    {
        case PLUS_X:  m_curPos.x += 1; break;
        case MINUS_X: m_curPos.x -= 1; break;
        case PLUS_Y:  m_curPos.y += 1; break;
        case MINUS_Y: m_curPos.y -= 1; break;
        case PLUS_Z:  m_curPos.z += 1; break;
        case MINUS_Z: m_curPos.z -= 1; break;
    }
}




//-----------------------------------------------------------------------------
// Name: align_plusz()
// Desc: Aligns +z with the given direction
//-----------------------------------------------------------------------------
void PIPE::align_plusz( int newDir )
{
    static D3DXVECTOR3 xAxis( 1.0f, 0.0f, 0.0f );
    static D3DXVECTOR3 yAxis( 0.0f, 1.0f, 0.0f );

    switch( newDir )
    {
        case PLUS_X:
            m_pWorldMatrixStack->RotateAxisLocal( &yAxis, PI / 2.0f );
            break;
        case MINUS_X:
            m_pWorldMatrixStack->RotateAxisLocal( &yAxis, -PI / 2.0f );
            break;
        case PLUS_Y:
            m_pWorldMatrixStack->RotateAxisLocal( &xAxis, -PI / 2.0f );
            break;
        case MINUS_Y:
            m_pWorldMatrixStack->RotateAxisLocal( &xAxis, PI / 2.0f );
            break;
        case PLUS_Z:
            m_pWorldMatrixStack->RotateAxisLocal( &yAxis, 0.0f );
            break;
        case MINUS_Z:
            m_pWorldMatrixStack->RotateAxisLocal( &yAxis, PI );
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: ChooseNewDirection()
// Desc: Chooses the next direction and takes the node it leads to.
//       Returns DIR_NONE if the pipe is stuck.
//-----------------------------------------------------------------------------
int PIPE::ChooseNewDirection()
{
    NODE_ARRAY* nodes = m_pState->m_nodes;
    int bestDirs[NUM_DIRS];
    int nBestDirs;

    // Chasing pipes go towards the lead pipe if they can
    if( m_chooseDirMethod == CHOOSE_DIR_CHASE )
    {
        nBestDirs = GetBestDirsForChase( bestDirs );
        if( nBestDirs )
            return nodes->ChoosePreferredDirection( &m_curPos, m_lastDir, bestDirs, nBestDirs );
    }

    return nodes->ChooseRandomDirection( &m_curPos, m_lastDir, m_weightStraight );
}
