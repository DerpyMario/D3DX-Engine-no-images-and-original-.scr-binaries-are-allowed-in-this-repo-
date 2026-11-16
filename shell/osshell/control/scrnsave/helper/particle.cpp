//-----------------------------------------------------------------------------
// File: particle.cpp
//
// Desc: 2D particles and a uniform grid that buckets them.
//
//       Original object (819 bytes of code, linked into Bubbles.scr):
//           shell\osshell\control\scrnsave\helper\objfre\amd64\particle.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <new>
#include "particle.h"




//-----------------------------------------------------------------------------
// Name: Grid2D::Create()
//-----------------------------------------------------------------------------
HRESULT __cdecl Grid2D::Create( Grid2D** ppGrid, Point2f ptMin, Point2f ptMax, int nCellsX, int nCellsY )
{
    HRESULT hr = E_OUTOFMEMORY;

    Grid2D* pGrid = new( std::nothrow ) Grid2D( ptMin, ptMax, nCellsX, nCellsY );
    if( pGrid != NULL )
    {
        pGrid->m_ppCells = new( std::nothrow ) Particle2DList*[pGrid->m_nCellsX];
        hr = E_OUTOFMEMORY;
        if( pGrid->m_ppCells != NULL )
        {
            int i;

            for( i = 0; i < pGrid->m_nCellsX; i++ )
                pGrid->m_ppCells[i] = NULL;

            hr = S_OK;
            for( i = 0; i < pGrid->m_nCellsX && SUCCEEDED( hr ); i++ )
            {
                pGrid->m_ppCells[i] = new( std::nothrow ) Particle2DList[pGrid->m_nCellsY];
                if( pGrid->m_ppCells[i] == NULL )
                    hr = E_OUTOFMEMORY;
            }

            if( SUCCEEDED( hr ) )
            {
                pGrid->m_fCellWidth  = ( pGrid->m_ptMax.x - pGrid->m_ptMin.x ) / (float)nCellsX;
                pGrid->m_fCellHeight = ( pGrid->m_ptMax.y - pGrid->m_ptMin.y ) / (float)nCellsY;
            }
        }

        if( FAILED( hr ) )
        {
            delete pGrid;
            pGrid = NULL;
        }
    }

    *ppGrid = pGrid;
    return hr;
}




//-----------------------------------------------------------------------------
// Name: Grid2D::AddParticle()
//-----------------------------------------------------------------------------
void Grid2D::AddParticle( Particle2D& particle )
{
    MapPoint( particle.m_pos, particle.m_nCellX, particle.m_nCellY );

    m_ppCells[particle.m_nCellX][particle.m_nCellY].AddTail( &particle );
}




//-----------------------------------------------------------------------------
// Name: Grid2D::MoveParticle()
//-----------------------------------------------------------------------------
void Grid2D::MoveParticle( Particle2D& particle )
{
    int nCellX;
    int nCellY;

    MapPoint( particle.m_pos, nCellX, nCellY );

    if( nCellX != particle.m_nCellX || nCellY != particle.m_nCellY )
    {
        m_ppCells[particle.m_nCellX][particle.m_nCellY].Remove( &particle );
        AddParticle( particle );
    }
}




//-----------------------------------------------------------------------------
// Name: Grid2D::MapPoint()
//-----------------------------------------------------------------------------
void Grid2D::MapPoint( const Point2f& pt, int& nCellX, int& nCellY )
{
    nCellX = (int)( ( pt.x - m_ptMin.x ) / m_fCellWidth );
    nCellY = (int)( ( pt.y - m_ptMin.y ) / m_fCellHeight );

    if( nCellX < 0 )
        nCellX = 0;
    else if( nCellX >= m_nCellsX )
        nCellX = m_nCellsX - 1;

    if( nCellY < 0 )
        nCellY = 0;
    else if( nCellY >= m_nCellsY )
        nCellY = m_nCellsY - 1;
}
