//-----------------------------------------------------------------------------
// File: particle.h
//
// Desc: 2D particles and a uniform grid that buckets them, so that a particle
//       only has to be tested against its neighbours.
//
//       Known from the symbols of Bubbles.scr: the class names DoubleLinkedList,
//       Particle2DList, Particle2D, Point2f and Grid2D, and the four Grid2D
//       member functions.  Everything that was inlined (constructors,
//       destructors, the list operations) and all member names are
//       reconstructions.
//
//       Original object:
//           shell\osshell\control\scrnsave\helper\objfre\amd64\particle.obj
//-----------------------------------------------------------------------------
#ifndef _HELPER_PARTICLE_H
#define _HELPER_PARTICLE_H

#include <windows.h>
#include "point.h"

//-----------------------------------------------------------------------------
// Name: class Particle2D
// Desc: A particle for Verlet integration that is also a node of an intrusive
//       doubly linked list (one list per grid cell).
//-----------------------------------------------------------------------------
class Particle2D
{
public:
    Particle2D()
    {
        m_pPrev   = NULL;
        m_pNext   = NULL;
        m_pos     = Point2f( 0.0f, 0.0f );
        m_prevPos = Point2f( 0.0f, 0.0f );
        m_fRadius = 1.0f;
        m_fMass   = 1.0f;
        m_force   = Point2f( 0.0f, 0.0f );
        m_nCellX  = 0;
        m_nCellY  = 0;
    }

    Particle2D* m_pNext;
    Particle2D* m_pPrev;
    Point2f     m_pos;
    Point2f     m_prevPos;      // position one time step ago
    Point2f     m_force;
    float       m_fMass;
    float       m_fRadius;
    int         m_nCellX;       // grid cell the particle is currently filed under
    int         m_nCellY;
};

//-----------------------------------------------------------------------------
// Name: class DoubleLinkedList / class Particle2DList
// Desc: The list classes have a single virtual function.  It is pure in the
//       base class and returns zero in Particle2DList; nothing in Bubbles.scr
//       calls it, so its real name and purpose are unknown.
//-----------------------------------------------------------------------------
class DoubleLinkedList
{
public:
    DoubleLinkedList()
    {
        m_pHead = NULL;
        m_pTail = NULL;
    }

    ~DoubleLinkedList() { }

    virtual INT_PTR Reserved() = 0;

    Particle2D* m_pHead;
    Particle2D* m_pTail;
};

class Particle2DList : public DoubleLinkedList
{
public:
    Particle2DList()
    {
        m_pReserved = NULL;
    }

    virtual INT_PTR Reserved()  { return 0; }

    // Appends a particle to the end of the list
    void AddTail( Particle2D* pParticle )
    {
        if( m_pHead == NULL )
        {
            m_pHead = pParticle;
            m_pTail = pParticle;
            pParticle->m_pNext = NULL;
            pParticle->m_pPrev = NULL;
        }
        else
        {
            m_pTail->m_pNext = pParticle;
            pParticle->m_pNext = NULL;
            pParticle->m_pPrev = m_pTail;
            m_pTail = pParticle;
        }
    }

    // Unlinks a particle.  (sic) Removing the only element leaves m_pTail
    // dangling; AddTail() only looks at m_pHead, so this is harmless.
    void Remove( Particle2D* pParticle )
    {
        if( pParticle == m_pHead )
        {
            m_pHead = m_pHead->m_pNext;
            if( m_pHead != NULL )
                m_pHead->m_pPrev = NULL;
        }
        else if( pParticle == m_pTail )
        {
            m_pTail = m_pTail->m_pPrev;
            m_pTail->m_pNext = NULL;
        }
        else
        {
            pParticle->m_pPrev->m_pNext = pParticle->m_pNext;
            pParticle->m_pNext->m_pPrev = pParticle->m_pPrev;
        }
    }

    void* m_pReserved;      // zeroed by the constructor, never used
};

//-----------------------------------------------------------------------------
// Name: class Grid2D
// Desc: nCellsX x nCellsY buckets that cover the rectangle [ptMin, ptMax].
//-----------------------------------------------------------------------------
class Grid2D
{
public:
    static HRESULT __cdecl Create( Grid2D** ppGrid, Point2f ptMin, Point2f ptMax, int nCellsX, int nCellsY );

    ~Grid2D()
    {
        if( m_ppCells != NULL )
        {
            for( int i = 0; i < m_nCellsX; i++ )
                delete[] m_ppCells[i];
            delete[] m_ppCells;
        }
    }

    // Files a particle under the cell that contains its position
    void AddParticle( Particle2D& particle );

    // Re-files a particle if it has left its cell
    void MoveParticle( Particle2D& particle );

    // Cell that contains a point (clamped to the grid)
    void MapPoint( const Point2f& pt, int& nCellX, int& nCellY );

    Particle2DList& Cell( int nCellX, int nCellY )  { return m_ppCells[nCellX][nCellY]; }

private:
    Grid2D( Point2f ptMin, Point2f ptMax, int nCellsX, int nCellsY )
    {
        m_ptMin   = ptMin;
        m_ptMax   = ptMax;
        m_nCellsX = nCellsX;
        m_nCellsY = nCellsY;
        m_ppCells = NULL;
    }

    Particle2DList** m_ppCells;     // m_ppCells[x][y]
    Point2f          m_ptMin;
    Point2f          m_ptMax;
    int              m_nCellsX;
    int              m_nCellsY;
    float            m_fCellWidth;
    float            m_fCellHeight;
};

#if defined(_WIN64)
static_assert( sizeof(Particle2D) == 0x38,     "Particle2D layout" );
static_assert( sizeof(Particle2DList) == 0x20, "Particle2DList layout" );
static_assert( sizeof(Grid2D) == 0x28,         "Grid2D layout" );
#endif

#endif // _HELPER_PARTICLE_H
