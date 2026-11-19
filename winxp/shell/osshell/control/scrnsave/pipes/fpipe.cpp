//-----------------------------------------------------------------------------
// File: fpipe.cpp
//
// Desc: FLEX_PIPE: the flexible pipes, made of extruded cross sections.
//
//       REGULAR_FLEX_PIPE goes from node to node like the normal pipes, with
//       turns of varying radius.  TURNING_FLEX_PIPE ("turnomania") turns all
//       the time.
//
//       (The flexible pipes are not drawn by this Direct3D port: see
//       eval.cpp.  They are not in the settings either.)
//
//       Reconstructed from fpipe.obj:
//           d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\fpipe.obj
//-----------------------------------------------------------------------------
#include "stdafx.h"

#define iXX     -1          // invalid entry

// Caps
#define START_CAP           0
#define END_CAP             1

// The cross sections have 4 arcs
#define EVAL_XC_CIRC_SECTION_COUNT  4

// Rounds a positive number up to the next integer
#define SS_ROUND_UP( fnum ) \
    ( ((fnum) - (int)(fnum)) > 0.0f ? (int)((fnum) + 1.0f) : (int)(fnum) )

// Clamps a to [lo, hi]
#define SS_CLAMP_TO_RANGE2( a, lo, hi ) \
    if( (a) < (lo) )                    \
        (a) = (lo);                     \
    else if( (a) > (hi) )               \
        (a) = (hi);


// The direction of the notch after align_plusz( newDir )
static int defCylNotch[NUM_DIRS] =
        { MINUS_Z, PLUS_Z, PLUS_X, PLUS_X, PLUS_X, MINUS_X };

// The direction of a turn relative to the cross section (PLUS_X .. MINUS_Y),
// per [lastDir][notchVec][newDir]
static int relDir[NUM_DIRS][NUM_DIRS][NUM_DIRS] =
{
    // last PLUS_X
    { { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , PLUS_X , MINUS_X, PLUS_Y , MINUS_Y },
      { iXX    , iXX    , MINUS_X, PLUS_X , MINUS_Y, PLUS_Y  },
      { iXX    , iXX    , MINUS_Y, PLUS_Y , PLUS_X , MINUS_X },
      { iXX    , iXX    , PLUS_Y , MINUS_Y, MINUS_X, PLUS_X  } },
    // last MINUS_X
    { { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , PLUS_X , MINUS_X, MINUS_Y, PLUS_Y  },
      { iXX    , iXX    , MINUS_X, PLUS_X , PLUS_Y , MINUS_Y },
      { iXX    , iXX    , PLUS_Y , MINUS_Y, PLUS_X , MINUS_X },
      { iXX    , iXX    , MINUS_Y, PLUS_Y , MINUS_X, PLUS_X  } },
    // last PLUS_Y
    { { PLUS_X , MINUS_X, iXX    , iXX    , MINUS_Y, PLUS_Y  },
      { MINUS_X, PLUS_X , iXX    , iXX    , PLUS_Y , MINUS_Y },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { PLUS_Y , MINUS_Y, iXX    , iXX    , PLUS_X , MINUS_X },
      { MINUS_Y, PLUS_Y , iXX    , iXX    , MINUS_X, PLUS_X  } },
    // last MINUS_Y
    { { PLUS_X , MINUS_X, iXX    , iXX    , PLUS_Y , MINUS_Y },
      { MINUS_X, PLUS_X , iXX    , iXX    , MINUS_Y, PLUS_Y  },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { MINUS_Y, PLUS_Y , iXX    , iXX    , PLUS_X , MINUS_X },
      { PLUS_Y , MINUS_Y, iXX    , iXX    , MINUS_X, PLUS_X  } },
    // last PLUS_Z
    { { PLUS_X , MINUS_X, PLUS_Y , MINUS_Y, iXX    , iXX     },
      { MINUS_X, PLUS_X , MINUS_Y, PLUS_Y , iXX    , iXX     },
      { MINUS_Y, PLUS_Y , PLUS_X , MINUS_X, iXX    , iXX     },
      { PLUS_Y , MINUS_Y, MINUS_X, PLUS_X , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     } },
    // last MINUS_Z
    { { PLUS_X , MINUS_X, MINUS_Y, PLUS_Y , iXX    , iXX     },
      { MINUS_X, PLUS_X , PLUS_Y , MINUS_Y, iXX    , iXX     },
      { PLUS_Y , MINUS_Y, PLUS_X , MINUS_X, iXX    , iXX     },
      { MINUS_Y, PLUS_Y , MINUS_X, PLUS_X , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     },
      { iXX    , iXX    , iXX    , iXX    , iXX    , iXX     } },
};

// Half the range of the turn factors of a REGULAR_FLEX_PIPE
static FLOAT turnFactorRange = 0.1f;




//-----------------------------------------------------------------------------
// Name: FLEX_PIPE()
// Desc:
//-----------------------------------------------------------------------------
FLEX_PIPE::FLEX_PIPE( STATE* pState ) : PIPE( pState )
{
    m_nSlices = pState->m_nSlices;

    m_xcCur = m_xcEnd = NULL;

    m_pEval = new EVAL( m_pState->m_bUseTexture );

    // The length of an evaluator section: about the size of a slice
    m_evalDivSize = pState->m_radius * (2.0f * PI) / (FLOAT) m_nSlices;
}




//-----------------------------------------------------------------------------
// Name: SetTexParams()
// Desc: (The texture coordinates are not set up by this port)
//-----------------------------------------------------------------------------
void FLEX_PIPE::SetTexParams( TEXTUREINFO* pTex, IPOINT2D* pTexRep )
{
}




//-----------------------------------------------------------------------------
// Name: ChooseXCProfile()
// Desc: Chooses the cross section of the pipe: an ellipse 3 times out of 4,
//       else a random one
//-----------------------------------------------------------------------------
void FLEX_PIPE::ChooseXCProfile()
{
    FLOAT baseRadius = m_pState->m_radius;

    // Evaluator parameters
    m_pEval->m_numSections = EVAL_XC_CIRC_SECTION_COUNT;
    m_pEval->m_uOrder = EVAL_ARC_ORDER;

    // u divisions per section
    m_pEval->m_uDiv = m_nSlices / m_pEval->m_numSections;

    if( CPipesScreensaver::iRand( 4 ) )
    {
        // ellipse
        FLOAT r2 = baseRadius;
        FLOAT r1 = CPipesScreensaver::fRand( 1.2f, 2.0f ) * baseRadius;
        m_xcCur = new ELLIPTICAL_XC( r1, r2 );
    }
    else
    {
        m_xcCur = new RANDOM4ARC_XC( CPipesScreensaver::fRand( 1.5f, 2.0f ) * baseRadius );
    }
}




//-----------------------------------------------------------------------------
// Name: CalcEvalLengthParams()
// Desc: The v divisions and the texture coordinates of a piece of the pipe
//-----------------------------------------------------------------------------
void FLEX_PIPE::CalcEvalLengthParams( FLOAT length )
{
    m_pEval->m_vDiv = (int) SS_ROUND_UP( length / m_evalDivSize );

    if( m_pState->m_bUseTexture )
    {
        // Keep s small
        if( m_s_end > 1.0f )
            m_s_end -= (int) m_s_end;

        m_s_start = m_s_end;
        m_s_end = m_s_start + (length / m_s_length);

        m_pEval->SetTextureControlPoints( m_s_start, m_s_end, m_t_start, m_t_end );
    }
}




//-----------------------------------------------------------------------------
// Name: ~FLEX_PIPE()
// Desc:
//-----------------------------------------------------------------------------
FLEX_PIPE::~FLEX_PIPE()
{
    delete m_pEval;

    // The cross sections (m_xcEnd may be m_xcCur)
    if( m_xcCur != NULL )
    {
        if( m_xcEnd == m_xcCur )
            m_xcEnd = NULL;
        delete m_xcCur;
        m_xcCur = NULL;
    }

    if( m_xcEnd != NULL )
    {
        delete m_xcEnd;
        m_xcEnd = NULL;
    }
}




//-----------------------------------------------------------------------------
// Name: REGULAR_FLEX_PIPE()
// Desc:
//-----------------------------------------------------------------------------
REGULAR_FLEX_PIPE::REGULAR_FLEX_PIPE( STATE* pState ) : FLEX_PIPE( pState )
{
    m_type = TYPE_FLEX_REGULAR;

    // The range of the turn factors
    FLOAT avgTurn = CPipesScreensaver::fRand( 0.11f, 0.81f );

    m_turnFactorMin = avgTurn - turnFactorRange;
    SS_CLAMP_TO_RANGE2( m_turnFactorMin, 0.0f, 1.0f );
    m_turnFactorMax = avgTurn + turnFactorRange;
    SS_CLAMP_TO_RANGE2( m_turnFactorMax, 0.0f, 1.0f );

    // The weight of going straight on
    if( !CPipesScreensaver::iRand( 20 ) )
        m_weightStraight = CPipesScreensaver::iRand2( 25, 100 );
    else
        m_weightStraight = CPipesScreensaver::iRand( 4 );
}




//-----------------------------------------------------------------------------
// Name: TURNING_FLEX_PIPE()
// Desc:
//-----------------------------------------------------------------------------
TURNING_FLEX_PIPE::TURNING_FLEX_PIPE( STATE* pState ) : FLEX_PIPE( pState )
{
    m_type = TYPE_FLEX_TURNING;
}




//-----------------------------------------------------------------------------
// Name: DrawXCElbow()
// Desc: A 90 degree turn of the given radius towards newDir
//-----------------------------------------------------------------------------
void FLEX_PIPE::DrawXCElbow( int newDir, FLOAT radius )
{
    int relativeDir;
    FLOAT length;

    // A quarter of a circle
    length = (2.0f * PI * radius) / 4.0f;
    CalcEvalLengthParams( length );

    m_pEval->m_vOrder = EVAL_ARC_ORDER;

    // The direction relative to the cross section
    relativeDir = relDir[m_lastDir][m_notchVec][newDir];

    m_pEval->ProcessXCPrimBendSimple( m_xcCur, relativeDir, radius );

    // Where the notch points to now
    m_notchVec = notchTurn[m_lastDir][newDir][m_notchVec];
}




//-----------------------------------------------------------------------------
// Name: DrawExtrudedXCObject()
// Desc: A straight piece of the given length
//-----------------------------------------------------------------------------
void FLEX_PIPE::DrawExtrudedXCObject( FLOAT length )
{
    CalcEvalLengthParams( length );

    m_pEval->m_vOrder = EVAL_CYLINDER_ORDER;

    m_pEval->ProcessXCPrimLinear( m_xcCur, m_xcCur, length );
}




//-----------------------------------------------------------------------------
// Name: DrawCap()
// Desc: Opens (START_CAP) or closes (END_CAP) the pipe
//-----------------------------------------------------------------------------
void FLEX_PIPE::DrawCap( int type )
{
    BOOL bOpening = (type == START_CAP) ? TRUE : FALSE;
    XC* xc = m_xcCur;

    // The average 'radius' of the cross section
    FLOAT radius = ((xc->m_xRight - xc->m_xLeft) + (xc->m_yTop - xc->m_yBottom)) / 4.0f;

    // A quarter of a circle
    FLOAT length = (2.0f * PI * radius) / 4.0f;
    CalcEvalLengthParams( length );

    m_pEval->m_vOrder = EVAL_ARC_ORDER;

    m_pEval->ProcessXCPrimSingularity( xc, radius, bOpening );
}




//-----------------------------------------------------------------------------
// Name: REGULAR_FLEX_PIPE::Start()
// Desc:
//-----------------------------------------------------------------------------
void REGULAR_FLEX_PIPE::Start()
{
    int newDir;

    // Find an empty node to start from
    if( !SetStartPos() )
    {
        m_status = PIPE_OUT_OF_NODES;
        return;
    }

    ChooseMaterial();
    ChooseXCProfile();

    TranslateToCurrentPosition();

    // Any direction is fine for the first node
    m_lastDir = CPipesScreensaver::iRand( NUM_DIRS );

    newDir = ChooseNewDirection();
    if( newDir == DIR_NONE )
    {
        // Nowhere to go from here
        m_status = PIPE_STUCK;
        return;
    }

    m_status = PIPE_ACTIVE;

    align_plusz( newDir );

    DrawCap( START_CAP );

    m_notchVec = defCylNotch[newDir];

    // The end of the pipe: one node back from the next node
    m_zTrans = - m_pState->m_view.m_divSize;

    UpdateCurrentPosition( newDir );
    m_lastDir = newDir;
}




//-----------------------------------------------------------------------------
// Name: TURNING_FLEX_PIPE::Start()
// Desc:
//-----------------------------------------------------------------------------
void TURNING_FLEX_PIPE::Start()
{
    NODE_ARRAY* nodes = m_pState->m_nodes;
    int newDir;

    // Find an empty node to start from
    if( !SetStartPos() )
    {
        m_status = PIPE_OUT_OF_NODES;
        return;
    }

    ChooseMaterial();
    ChooseXCProfile();

    TranslateToCurrentPosition();

    // Any direction is fine for the first node
    m_lastDir = CPipesScreensaver::iRand( NUM_DIRS );

    newDir = nodes->FindClearestDirection( &m_curPos );
    if( newDir == DIR_NONE )
    {
        // Nowhere to go from here
        m_status = PIPE_STUCK;
        return;
    }

    m_status = PIPE_ACTIVE;

    align_plusz( newDir );

    DrawCap( START_CAP );

    m_notchVec = defCylNotch[newDir];
    m_zTrans = 0.0f;
    m_lastDir = newDir;
}




//-----------------------------------------------------------------------------
// Name: REGULAR_FLEX_PIPE::Draw()
// Desc: Draws the pipe to the next node: a straight piece, or a straight
//       piece and a turn of random radius.  m_zTrans is where the end of
//       the pipe is, relative to the current node (negative: behind it).
//-----------------------------------------------------------------------------
void REGULAR_FLEX_PIPE::Draw()
{
    FLOAT divSize = m_pState->m_view.m_divSize;
    FLOAT turnFactor;
    FLOAT minLen, maxLen, pipeLen;
    int newDir;

    newDir = ChooseNewDirection();
    if( newDir == DIR_NONE )
    {
        // The pipe is stuck: close it
        m_status = PIPE_STUCK;
        DrawCap( END_CAP );
        return;
    }

    if( newDir == m_lastDir )
    {
        // Straight on: at least to the current node, and leave room for a
        // turn at the next one
        minLen = -m_zTrans;
        maxLen = minLen + divSize - m_xcCur->MaxExtent();

        turnFactor = CPipesScreensaver::fRand( 1.0f - m_turnFactorMax, 1.0f - m_turnFactorMin );
        pipeLen = minLen + turnFactor * (maxLen - minLen);

        DrawExtrudedXCObject( pipeLen );

        m_zTrans += pipeLen - divSize;
    }
    else
    {
        // A turn: a straight piece, then the elbow
        FLOAT minTurnRadius = m_xcCur->MinTurnRadius( relDir[m_lastDir][m_notchVec][newDir] );
        FLOAT turnRadius;

        // The longest piece leaves the smallest turn; the shortest piece
        // keeps the pipe out of the way of the nodes around
        maxLen = -m_zTrans - minTurnRadius;
        minLen = m_xcCur->MaxExtent() - (divSize + m_zTrans);
        if( minLen < 0.0f )
            minLen = 0.0f;

        turnFactor = CPipesScreensaver::fRand( 1.0f - m_turnFactorMax, 1.0f - m_turnFactorMin );
        pipeLen = minLen + turnFactor * (maxLen - minLen);
        turnRadius = maxLen - pipeLen + minTurnRadius;

        DrawExtrudedXCObject( pipeLen );
        m_zTrans += pipeLen;

        DrawXCElbow( newDir, turnRadius );

        // The end of the elbow, relative to the next node
        m_zTrans = - (divSize - turnRadius);
    }

    UpdateCurrentPosition( newDir );
    m_lastDir = newDir;
}




//-----------------------------------------------------------------------------
// Name: TURNING_FLEX_PIPE::Draw()
// Desc: Turns at every other node, if it can
//-----------------------------------------------------------------------------
void TURNING_FLEX_PIPE::Draw()
{
    NODE_ARRAY* nodes = m_pState->m_nodes;
    FLOAT divSize = m_pState->m_view.m_divSize;
    int newDir;

    newDir = nodes->ChooseNewTurnDirection( &m_curPos, m_lastDir );
    if( newDir == DIR_NONE )
    {
        // The pipe is stuck: close it
        m_status = PIPE_STUCK;
        DrawCap( END_CAP );
        return;
    }

    if( newDir == DIR_STRAIGHT )
    {
        // No turn possible: straight on to the next node
        DrawExtrudedXCObject( divSize );
        UpdateCurrentPosition( m_lastDir );
        nodes->NodeVisited( &m_curPos );
        return;
    }

    // A turn through the next node
    DrawXCElbow( newDir, divSize );

    UpdateCurrentPosition( m_lastDir );
    UpdateCurrentPosition( newDir );

    m_lastDir = newDir;
}
