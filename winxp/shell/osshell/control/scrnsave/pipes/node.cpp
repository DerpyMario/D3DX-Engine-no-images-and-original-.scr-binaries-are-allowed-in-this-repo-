//-----------------------------------------------------------------------------
// File: node.cpp
//
// Desc: NODE_ARRAY: the 3D grid of nodes the pipes run through.  A node is
//       empty until a pipe goes through it.
//
//       Reconstructed from node.obj:
//           d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\node.obj
//-----------------------------------------------------------------------------
#include "stdafx.h"

// Random tries before FindRandomEmptyNode() searches the whole grid
// (twice the number of nodes of a 15 x 15 x 15 grid)
#define MAX_RANDOM_TRIES        6750

// Weight of going straight that makes ChooseRandomDirection() go straight
#define MAX_WEIGHT_STRAIGHT     100




//-----------------------------------------------------------------------------
// Name: NODE_ARRAY()
// Desc:
//-----------------------------------------------------------------------------
NODE_ARRAY::NODE_ARRAY()
{
    m_nodes = NULL;
    m_numNodes.x = 0;
    m_numNodes.y = 0;
    m_numNodes.z = 0;
}

NODE_ARRAY::~NODE_ARRAY()
{
    if( m_nodes )
        delete [] m_nodes;
}




//-----------------------------------------------------------------------------
// Name: Resize()
// Desc: Makes a new, empty grid of the given size
//-----------------------------------------------------------------------------
void NODE_ARRAY::Resize( IPOINT3D* pNewSize )
{
    if( m_numNodes.x == pNewSize->x &&
        m_numNodes.y == pNewSize->y &&
        m_numNodes.z == pNewSize->z )
        return;

    m_numNodes = *pNewSize;

    int elemCount = m_numNodes.x * m_numNodes.y * m_numNodes.z;

    if( m_nodes )
        delete [] m_nodes;

    m_nodes = new Node[elemCount];
    if( m_nodes == NULL )
        return;

    // Reset the node states to empty
    for( int i = 0; i < elemCount; i++ )
        m_nodes[i].empty = TRUE;

    // Precalculate the offsets between neighbouring nodes
    m_nodeDirInc[MINUS_X] = -1;
    m_nodeDirInc[PLUS_X] = 1;
    m_nodeDirInc[MINUS_Y] = -m_numNodes.x;
    m_nodeDirInc[PLUS_Z] = m_numNodes.x * m_numNodes.y;
    m_nodeDirInc[PLUS_Y] = m_numNodes.x;
    m_nodeDirInc[MINUS_Z] = -m_nodeDirInc[PLUS_Z];
}




//-----------------------------------------------------------------------------
// Name: Reset()
// Desc: Empties all the nodes
//-----------------------------------------------------------------------------
void NODE_ARRAY::Reset()
{
    Node* pNode = m_nodes;

    for( int i = 0; i < m_numNodes.x * m_numNodes.y * m_numNodes.z; i++, pNode++ )
        pNode->empty = TRUE;
}




//-----------------------------------------------------------------------------
// Name: GetNodeCount()
// Desc:
//-----------------------------------------------------------------------------
void NODE_ARRAY::GetNodeCount( IPOINT3D* pos )
{
    *pos = m_numNodes;
}




//-----------------------------------------------------------------------------
// Name: GetNode()
// Desc:
//-----------------------------------------------------------------------------
Node* NODE_ARRAY::GetNode( IPOINT3D* pos )
{
    return m_nodes + (pos->z * m_numNodes.y + pos->y) * m_numNodes.x + pos->x;
}




//-----------------------------------------------------------------------------
// Name: GetNextNode()
// Desc: The neighbour of a node in the given direction (NULL at the edge)
//-----------------------------------------------------------------------------
Node* NODE_ARRAY::GetNextNode( IPOINT3D* pos, int dir )
{
    Node* curNode = GetNode( pos );

    switch( dir )
    {
        case PLUS_X:
            if( pos->x == m_numNodes.x - 1 )
                return NULL;
            return curNode + m_nodeDirInc[PLUS_X];
        case MINUS_X:
            if( pos->x == 0 )
                return NULL;
            return curNode + m_nodeDirInc[MINUS_X];
        case PLUS_Y:
            if( pos->y == m_numNodes.y - 1 )
                return NULL;
            return curNode + m_nodeDirInc[PLUS_Y];
        case MINUS_Y:
            if( pos->y == 0 )
                return NULL;
            return curNode + m_nodeDirInc[MINUS_Y];
        case PLUS_Z:
            if( pos->z == m_numNodes.z - 1 )
                return NULL;
            return curNode + m_nodeDirInc[PLUS_Z];
        case MINUS_Z:
            if( pos->z == 0 )
                return NULL;
            return curNode + m_nodeDirInc[MINUS_Z];
        default:
            return NULL;
    }
}




//-----------------------------------------------------------------------------
// Name: GetEmptyNeighbours()
// Desc: The directions of the empty neighbours
//-----------------------------------------------------------------------------
int NODE_ARRAY::GetEmptyNeighbours( Node** nNode, int* nEmpty )
{
    int count = 0;

    for( int i = 0; i < NUM_DIRS; i++ )
    {
        if( nNode[i] && nNode[i]->empty )
            nEmpty[count++] = i;
    }

    return count;
}




//-----------------------------------------------------------------------------
// Name: GetEmptyTurnNeighbours()
// Desc: The directions of the empty neighbours, except straight on
//-----------------------------------------------------------------------------
int NODE_ARRAY::GetEmptyTurnNeighbours( Node** nNode, int* nEmpty, int lastDir )
{
    int count = 0;

    for( int i = 0; i < NUM_DIRS; i++ )
    {
        if( nNode[i] && nNode[i]->empty && i != lastDir )
            nEmpty[count++] = i;
    }

    return count;
}




//-----------------------------------------------------------------------------
// Name: GetEmptyNeighboursAlongDir()
// Desc: How many empty nodes follow a node in the given direction, looking
//       at most searchRadius nodes far
//-----------------------------------------------------------------------------
int NODE_ARRAY::GetEmptyNeighboursAlongDir( IPOINT3D* pos, int dir, int searchRadius )
{
    Node* curNode = GetNode( pos );
    int nodeDirInc = m_nodeDirInc[dir];
    int count = 0;
    int maxSearch = 0;

    switch( dir )
    {
        case PLUS_X:  maxSearch = m_numNodes.x - pos->x - 1; break;
        case MINUS_X: maxSearch = pos->x; break;
        case PLUS_Y:  maxSearch = m_numNodes.y - pos->y - 1; break;
        case MINUS_Y: maxSearch = pos->y; break;
        case PLUS_Z:  maxSearch = m_numNodes.z - pos->z - 1; break;
        case MINUS_Z: maxSearch = pos->z; break;
    }

    if( searchRadius > maxSearch )
        searchRadius = maxSearch;

    if( !searchRadius )
        return 0;

    while( searchRadius-- )
    {
        curNode += nodeDirInc;
        if( !curNode->empty )
            return count;
        count++;
    }

    return count;
}




//-----------------------------------------------------------------------------
// Name: GetNeighbours()
// Desc: The six neighbours of a node (NULL beyond the edges)
//-----------------------------------------------------------------------------
void NODE_ARRAY::GetNeighbours( IPOINT3D* pos, Node** nNode )
{
    Node* curNode = GetNode( pos );

    nNode[PLUS_X]  = (pos->x == m_numNodes.x - 1) ? NULL : curNode + m_nodeDirInc[PLUS_X];
    nNode[PLUS_Y]  = (pos->y == m_numNodes.y - 1) ? NULL : curNode + m_nodeDirInc[PLUS_Y];
    nNode[PLUS_Z]  = (pos->z == m_numNodes.z - 1) ? NULL : curNode + m_nodeDirInc[PLUS_Z];
    nNode[MINUS_X] = (pos->x == 0) ? NULL : curNode + m_nodeDirInc[MINUS_X];
    nNode[MINUS_Y] = (pos->y == 0) ? NULL : curNode + m_nodeDirInc[MINUS_Y];
    nNode[MINUS_Z] = (pos->z == 0) ? NULL : curNode + m_nodeDirInc[MINUS_Z];
}




//-----------------------------------------------------------------------------
// Name: NodeVisited()
// Desc:
//-----------------------------------------------------------------------------
void NODE_ARRAY::NodeVisited( IPOINT3D* pos )
{
    GetNode( pos )->empty = FALSE;
}




//-----------------------------------------------------------------------------
// Name: GetNextNodePos()
// Desc: The position of the next node in the given direction, if it is
//       empty
//-----------------------------------------------------------------------------
BOOL NODE_ARRAY::GetNextNodePos( IPOINT3D* curPos, IPOINT3D* nextPos, int dir )
{
    static Node* nNode[NUM_DIRS];

    GetNeighbours( curPos, nNode );

    *nextPos = *curPos;

    if( !nNode[dir] || !nNode[dir]->empty )
        return FALSE;

    switch( dir )
    {
        case PLUS_X:  nextPos->x = curPos->x + 1; break;
        case MINUS_X: nextPos->x = curPos->x - 1; break;
        case PLUS_Y:  nextPos->y = curPos->y + 1; break;
        case MINUS_Y: nextPos->y = curPos->y - 1; break;
        case PLUS_Z:  nextPos->z = curPos->z + 1; break;
        case MINUS_Z: nextPos->z = curPos->z - 1; break;
    }

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: FindRandomEmptyNode()
// Desc: Takes a random empty node: random tries first, then the first empty
//       node of the grid.  Returns FALSE if there is none.
//-----------------------------------------------------------------------------
int NODE_ARRAY::FindRandomEmptyNode( IPOINT3D* pos )
{
    int nTries = 0;

    for( ;; )
    {
        pos->x = CPipesScreensaver::iRand( m_numNodes.x );
        pos->y = CPipesScreensaver::iRand( m_numNodes.y );
        pos->z = CPipesScreensaver::iRand( m_numNodes.z );

        if( GetNode( pos )->empty )
        {
            NodeVisited( pos );
            return TRUE;
        }

        if( nTries > MAX_RANDOM_TRIES )
            break;
        nTries++;
    }

    // Search the whole grid
    for( pos->x = 0; pos->x < m_numNodes.x; pos->x++ )
    {
        for( pos->y = 0; pos->y < m_numNodes.y; pos->y++ )
        {
            for( pos->z = 0; pos->z < m_numNodes.z; pos->z++ )
            {
                if( GetNode( pos )->empty )
                {
                    NodeVisited( pos );
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: FindRandomEmptyNode2D()
// Desc: Takes a random empty node on one face (plane) of a box.  box holds
//       the maximum and the minimum of each coordinate, in the order of the
//       directions.
//-----------------------------------------------------------------------------
int NODE_ARRAY::FindRandomEmptyNode2D( IPOINT3D* pos, int plane, int* box )
{
    int *newx, *newy;
    int *xLim, *yLim;

    switch( plane )
    {
        case PLUS_X:
        case MINUS_X:
            pos->x = box[plane];
            newx = &pos->z;
            newy = &pos->y;
            xLim = &box[PLUS_Z];
            yLim = &box[PLUS_Y];
            break;
        case PLUS_Y:
        case MINUS_Y:
            pos->y = box[plane];
            newx = &pos->x;
            newy = &pos->z;
            xLim = &box[PLUS_X];
            yLim = &box[PLUS_Z];
            break;
        case PLUS_Z:
        case MINUS_Z:
            pos->z = box[plane];
            newx = &pos->x;
            newy = &pos->y;
            xLim = &box[PLUS_X];
            yLim = &box[PLUS_Y];
            break;
        default:
            return FALSE;   // (left unhandled by the original)
    }

    // Random tries first: twice the number of nodes of the face
    int maxTries = (xLim[0] - xLim[1] + 1) * (yLim[0] - yLim[1] + 1) * 2;
    int nTries = 0;

    for( ;; )
    {
        *newx = CPipesScreensaver::iRand2( xLim[1], xLim[0] );
        *newy = CPipesScreensaver::iRand2( yLim[1], yLim[0] );

        if( GetNode( pos )->empty )
        {
            NodeVisited( pos );
            return TRUE;
        }

        if( ++nTries > maxTries )
            break;
    }

    // Search the whole face
    for( *newx = xLim[1]; *newx <= xLim[0]; (*newx)++ )
    {
        for( *newy = yLim[1]; *newy <= yLim[0]; (*newy)++ )
        {
            if( GetNode( pos )->empty )
            {
                NodeVisited( pos );
                return TRUE;
            }
        }
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: TakeClosestEmptyNode()
// Desc: Takes pos if it is empty, else an empty node on a box growing
//       around it, else any empty node.
//-----------------------------------------------------------------------------
BOOL NODE_ARRAY::TakeClosestEmptyNode( IPOINT3D* newPos, IPOINT3D* pos )
{
    static int searchRadius = max( m_numNodes.x, m_numNodes.y ) / 3;

    if( GetNode( pos )->empty )
    {
        NodeVisited( pos );
        *newPos = *pos;
        return TRUE;
    }

    int box[NUM_DIRS];
    box[PLUS_X] = box[MINUS_X] = pos->x;
    box[PLUS_Y] = box[MINUS_Y] = pos->y;
    box[PLUS_Z] = box[MINUS_Z] = pos->z;

    for( int i = 0; i < searchRadius; i++ )
    {
        // Grow the box by one node in every direction
        int* numNodes = &m_numNodes.x;
        for( int axis = 0; axis < 3; axis++ )
        {
            if( box[axis * 2 + 1] > 0 )
                box[axis * 2 + 1]--;
            if( box[axis * 2] < numNodes[axis] - 1 )
                box[axis * 2]++;
        }

        // Try its faces, starting with a random one
        int plane = CPipesScreensaver::iRand( NUM_DIRS );
        for( int j = 0; j < NUM_DIRS; j++ )
        {
            if( FindRandomEmptyNode2D( newPos, plane, box ) )
                return TRUE;
            plane = (plane == MINUS_Z) ? 0 : plane + 1;
        }
    }

    return FindRandomEmptyNode( newPos );
}




//-----------------------------------------------------------------------------
// Name: ChooseRandomDirection()
// Desc: Chooses an empty neighbour at random and takes it.  Going straight
//       on counts weight times as much as a turn.
//-----------------------------------------------------------------------------
int NODE_ARRAY::ChooseRandomDirection( IPOINT3D* pos, int dir, int weight )
{
    Node* nNode[NUM_DIRS];
    int emptyDirs[NUM_DIRS];
    Node* straightNode = NULL;
    int numEmpty, newDir, choice;

    GetNeighbours( pos, nNode );

    if( weight && nNode[dir] && nNode[dir]->empty )
    {
        straightNode = nNode[dir];

        // Maximum weight: always straight on
        if( weight == MAX_WEIGHT_STRAIGHT )
        {
            straightNode->empty = FALSE;
            return dir;
        }
    }
    else
    {
        weight = 0;
    }

    numEmpty = GetEmptyTurnNeighbours( nNode, emptyDirs, dir );

    if( numEmpty + weight == 0 )
        return DIR_NONE;

    choice = CPipesScreensaver::iRand( numEmpty + weight );
    if( choice < weight && straightNode )
    {
        straightNode->empty = FALSE;
        return dir;
    }

    newDir = emptyDirs[choice - weight];
    nNode[newDir]->empty = FALSE;
    return newDir;
}




//-----------------------------------------------------------------------------
// Name: ChoosePreferredDirection()
// Desc: Chooses one of the preferred directions (a sorted list) that leads
//       to an empty node, else any empty neighbour, and takes it.
//-----------------------------------------------------------------------------
int NODE_ARRAY::ChoosePreferredDirection( IPOINT3D* pos, int dir, int* prefDirs, int nPrefDirs )
{
    Node* nNode[NUM_DIRS];
    int emptyDirs[NUM_DIRS];
    int* pEmptyPrefDirs = emptyDirs;
    int numEmpty = 0;
    int newDir;
    int i, j;

    GetNeighbours( pos, nNode );

    for( i = 0, j = 0; (i < NUM_DIRS) && (j < nPrefDirs); i++ )
    {
        if( i == *prefDirs )
        {
            prefDirs++;
            j++;
            if( nNode[i] && nNode[i]->empty )
            {
                *pEmptyPrefDirs++ = i;
                numEmpty++;
            }
        }
    }

    // No preferred direction is free: any empty direction then
    if( !numEmpty )
    {
        numEmpty = GetEmptyNeighbours( nNode, emptyDirs );
        if( numEmpty == 0 )
            return DIR_NONE;
    }

    newDir = emptyDirs[CPipesScreensaver::iRand( numEmpty )];
    nNode[newDir]->empty = FALSE;
    return newDir;
}




//-----------------------------------------------------------------------------
// Name: FindClearestDirection()
// Desc: The direction with the most empty nodes ahead (up to 3), at random
//       among equals.  The node is not taken.
//-----------------------------------------------------------------------------
int NODE_ARRAY::FindClearestDirection( IPOINT3D* pos )
{
    static Node* neighbNode[NUM_DIRS];
    static int emptyDirs[NUM_DIRS];
    int nEmpty, maxEmpty = 0;
    int count = 0;

    GetNeighbours( pos, neighbNode );

    for( int i = 0; i < NUM_DIRS; i++ )
    {
        if( neighbNode[i] && neighbNode[i]->empty )
        {
            nEmpty = GetEmptyNeighboursAlongDir( pos, i, 3 );
            if( nEmpty > maxEmpty )
            {
                maxEmpty = nEmpty;
                count = 1;
                emptyDirs[0] = i;
            }
            else if( nEmpty == maxEmpty )
            {
                emptyDirs[count++] = i;
            }
        }
    }

    if( count == 0 )
        return DIR_NONE;

    return emptyDirs[CPipesScreensaver::iRand( count )];
}




//-----------------------------------------------------------------------------
// Name: GetBestPossibleTurns()
// Desc: The turns (not along dir) with the most empty nodes ahead (up to 2)
//-----------------------------------------------------------------------------
int NODE_ARRAY::GetBestPossibleTurns( IPOINT3D* pos, int dir, int* turnDirs )
{
    Node* nNode[NUM_DIRS];
    BOOL check[NUM_DIRS] = { TRUE, TRUE, TRUE, TRUE, TRUE, TRUE };
    int nEmpty, maxEmpty = 0;
    int count = 0;

    GetNeighbours( pos, nNode );

    switch( dir )
    {
        case PLUS_X:
        case MINUS_X:
            check[PLUS_X] = FALSE;
            check[MINUS_X] = FALSE;
            break;
        case PLUS_Y:
        case MINUS_Y:
            check[PLUS_Y] = FALSE;
            check[MINUS_Y] = FALSE;
            break;
        case PLUS_Z:
        case MINUS_Z:
            check[PLUS_Z] = FALSE;
            check[MINUS_Z] = FALSE;
            break;
    }

    for( int i = 0; i < NUM_DIRS; i++ )
    {
        if( check[i] && nNode[i] && nNode[i]->empty )
        {
            nEmpty = GetEmptyNeighboursAlongDir( pos, i, 2 );
            if( nEmpty > maxEmpty )
            {
                maxEmpty = nEmpty;
                count = 1;
                turnDirs[0] = i;
            }
            else if( nEmpty == maxEmpty )
            {
                turnDirs[count++] = i;
            }
        }
    }

    return count;
}




//-----------------------------------------------------------------------------
// Name: ChooseNewTurnDirection()
// Desc: (TURNING_FLEX_PIPE) Goes one node on in dir and turns from there.
//       Returns DIR_NONE if the next node is taken, DIR_STRAIGHT if no turn
//       is possible from it.
//-----------------------------------------------------------------------------
int NODE_ARRAY::ChooseNewTurnDirection( IPOINT3D* pos, int dir )
{
    IPOINT3D nextPos;
    int turnDirs[NUM_DIRS];
    int nTurns, newDir;
    Node* nextNode;

    if( !GetNextNodePos( pos, &nextPos, dir ) )
        return DIR_NONE;

    nTurns = GetBestPossibleTurns( &nextPos, dir, turnDirs );
    if( !nTurns )
        return DIR_STRAIGHT;

    newDir = turnDirs[CPipesScreensaver::iRand( nTurns )];

    // Take the next node and the one after the turn
    GetNode( &nextPos )->empty = FALSE;
    nextNode = GetNextNode( &nextPos, newDir );
    if( nextNode )
        nextNode->empty = FALSE;

    return newDir;
}
