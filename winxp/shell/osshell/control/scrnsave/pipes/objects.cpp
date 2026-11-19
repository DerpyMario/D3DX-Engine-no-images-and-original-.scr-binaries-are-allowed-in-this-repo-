//-----------------------------------------------------------------------------
// File: objects.cpp
//
// Desc: The objects the normal pipes are drawn with: pipes (cylinders),
//       elbows, ball joints and spheres, in vertex buffers.
//
//       The geometry is that of the OpenGL screen saver (the cylinder and the
//       sphere are those of gluCylinder and gluSphere): each object is built
//       as quad strips, then converted to a triangle list.
//
//       Reconstructed from objects.obj:
//           d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\objects.obj
//-----------------------------------------------------------------------------
#include "stdafx.h"

#define CACHE_SIZE      100     // most slices and stacks, plus one

// Rounds a positive number up to the next integer
#define SS_ROUND_UP( fnum ) \
    ( ((fnum) - (int)(fnum)) > 0.0f ? (int)((fnum) + 1.0f) : (int)(fnum) )




//-----------------------------------------------------------------------------
// Name: OBJECT()
// Desc:
//-----------------------------------------------------------------------------
OBJECT::OBJECT( IDirect3DDevice8* pd3dDevice )
{
    m_pVB = NULL;
    m_dwNumTriangles = 0;
    m_pd3dDevice = pd3dDevice;
}




//-----------------------------------------------------------------------------
// Name: ~OBJECT()
// Desc:
//-----------------------------------------------------------------------------
OBJECT::~OBJECT()
{
    SAFE_RELEASE( m_pVB );
}




//-----------------------------------------------------------------------------
// Name: Draw()
// Desc:
//-----------------------------------------------------------------------------
void OBJECT::Draw( D3DXMATRIX* pWorldMat )
{
    if( m_pVB )
    {
        m_pd3dDevice->SetTransform( D3DTS_WORLD, pWorldMat );
        m_pd3dDevice->SetVertexShader( D3DFVF_MYVERTEX );
        m_pd3dDevice->SetStreamSource( 0, m_pVB, sizeof(MYVERTEX) );
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, m_dwNumTriangles );
    }
}




//-----------------------------------------------------------------------------
// Name: TransformCircle()
// Desc: Rotates the points by angle (radians) about the x axis through center
//-----------------------------------------------------------------------------
static void TransformCircle( FLOAT angle, D3DXVECTOR3* inPoint, D3DXVECTOR3* outPoint,
                             int num, D3DXVECTOR3* center )
{
    D3DXMATRIX matrix1, matrix2, matrix3;
    D3DXVECTOR4 v;
    int i;

    // Move the center to the origin
    D3DXMatrixIdentity( &matrix1 );
    D3DXMatrixTranslation( &matrix1, -center->x, -center->y, -center->z );

    // Rotate
    D3DXMatrixIdentity( &matrix2 );
    D3DXMatrixRotationYawPitchRoll( &matrix2, 0.0f, angle, 0.0f );

    D3DXMatrixMultiply( &matrix3, &matrix1, &matrix2 );

    // Move back
    D3DXMatrixIdentity( &matrix2 );
    D3DXMatrixTranslation( &matrix2, center->x, center->y, center->z );

    D3DXMatrixMultiply( &matrix1, &matrix3, &matrix2 );

    // Transform the points
    for( i = 0; i < num; i++, inPoint++, outPoint++ )
    {
        D3DXVec3Transform( &v, inPoint, &matrix1 );
        outPoint->x = v.x;
        outPoint->y = v.y;
        outPoint->z = v.z;
    }
}




//-----------------------------------------------------------------------------
// Name: CalcNormals()
// Desc: The normals of points on a sphere around center
//-----------------------------------------------------------------------------
static void CalcNormals( D3DXVECTOR3* p, D3DXVECTOR3* n, D3DXVECTOR3* center, int num )
{
    int i;

    for( i = 0; i < num; i++, n++, p++ )
    {
        n->x = p->x - center->x;
        n->y = p->y - center->y;
        n->z = p->z - center->z;
        D3DXVec3Normalize( n, n );
    }
}




//-----------------------------------------------------------------------------
// Name: BALLJOINT_OBJECT::Build()
// Desc: A ball joint with the orientation of an elbow (for textures): the
//       rings of a sphere of radius ROOT_TWO*r from 45 to 135 degrees,
//       rotated like the rings of the elbow
//-----------------------------------------------------------------------------
void BALLJOINT_OBJECT::Build( OBJECT_BUILD_INFO* pBuildInfo, int notch, FLOAT s_start, FLOAT s_end )
{
    FLOAT       radius = pBuildInfo->radius;
    FLOAT       angle, delta_a, startAng, theta;
    int         numPoints;
    FLOAT       s_delta;
    int         stacks, slices;
    int         i, j;
    D3DXVECTOR3 pi[CACHE_SIZE];     // initial ring of points
    D3DXVECTOR3 pt[CACHE_SIZE];     // a ring before its rotation
    D3DXVECTOR3 p0[CACHE_SIZE];     // 2 rings of points
    D3DXVECTOR3 p1[CACHE_SIZE];
    D3DXVECTOR3 n0[CACHE_SIZE];     // 2 rings of normals
    D3DXVECTOR3 n1[CACHE_SIZE];
    FLOAT       r[CACHE_SIZE];      // radii of the rings
    FLOAT       tex_t[CACHE_SIZE];  // t texture coordinates
    FLOAT       tex_s[2];           // s texture coordinates
    D3DXVECTOR3 center;             // center of the ball
    D3DXVECTOR3 rotCenter;          // center of the rotations
    D3DXVECTOR3 *pPoints0, *pPoints1, *pNormals0, *pNormals1;
    IPOINT2D*   texRep = pBuildInfo->texRep;

    slices = pBuildInfo->nSlices;
    stacks = slices;

    if( slices >= CACHE_SIZE ) slices = CACHE_SIZE-1;
    if( stacks >= CACHE_SIZE ) stacks = CACHE_SIZE-1;

    // The radii of the rings
    theta = PI / 4;
    for( i = 0; i <= stacks; i++ )
    {
        r[i] = (FLOAT) sin( theta ) * radius * ROOT_TWO;
        theta += PI_OVER_2 / stacks;
    }

    // t texture coordinates
    for( i = 0; i <= slices; i++ )
        tex_t[i] = (FLOAT) i * texRep->y / slices;

    s_delta = s_end - s_start;

    numPoints = slices + 1;

    center = D3DXVECTOR3( 0.0f, 0.0f, -radius );

    // The initial ring: a circle in the xz plane, starting at the notch
    startAng = notch * PI / 2;
    delta_a = 2 * PI / slices;

    angle = startAng;
    for( i = 0; i <= slices; i++ )
    {
        pi[i].x = (FLOAT) sin( angle ) * r[0];
        pi[i].y = radius;
        pi[i].z = (FLOAT) cos( angle ) * r[0] - r[0];
        angle += delta_a;
    }

    // The rings are rotated about the x axis through (0, r, 0)
    rotCenter = D3DXVECTOR3( 0.0f, radius, 0.0f );

    CalcNormals( pi, n0, &center, numPoints );

    tex_s[0] = s_start;
    pPoints0 = pi;
    pPoints1 = p1;
    pNormals0 = n0;
    pNormals1 = n1;

    // Vertex buffer: 2 triangles per quad
    int numQuads = slices * stacks;
    m_dwNumTriangles = numQuads * 2;
    DWORD dwSize = numQuads * 6 * sizeof(MYVERTEX);

    if( FAILED( m_pd3dDevice->CreateVertexBuffer( dwSize, D3DUSAGE_WRITEONLY, D3DFVF_MYVERTEX,
                                                  D3DPOOL_MANAGED, &m_pVB ) ) )
        return;

    // The quad strips
    MYVERTEX* pVertices = new MYVERTEX[numQuads * 6];
    ZeroMemory( pVertices, dwSize );
    MYVERTEX* pVertex = pVertices;

    for( i = 1; i <= stacks; i++ )
    {
        // The ring of this stack, rotated into place
        angle = startAng;
        for( j = 0; j <= slices; j++ )
        {
            pt[j].y = radius;
            pt[j].x = (FLOAT) sin( angle ) * r[i];
            pt[j].z = (FLOAT) cos( angle ) * r[i] - r[i];
            angle += delta_a;
        }

        TransformCircle( (FLOAT) i * -PI_OVER_2 / stacks, pt, pPoints1, numPoints, &rotCenter );

        CalcNormals( pPoints1, pNormals1, &center, numPoints );

        tex_s[1] = (FLOAT) i * s_delta / stacks + s_start;

        // A quad strip between the last ring and this one
        for( j = 0; j < numPoints; j++ )
        {
            pVertex->p = pPoints0[j];
            pVertex->n = pNormals0[j];
            if( texRep )
            {
                pVertex->tu = tex_s[0];
                pVertex->tv = tex_t[j];
            }
            pVertex++;

            pVertex->p = pPoints1[j];
            pVertex->n = pNormals1[j];
            if( texRep )
            {
                pVertex->tu = tex_s[1];
                pVertex->tv = tex_t[j];
            }
            pVertex++;
        }

        // Swap the buffers
        pPoints0 = pPoints1;
        pPoints1 = (pPoints1 == p1) ? p0 : p1;
        pNormals0 = pNormals1;
        pNormals1 = (pNormals1 == n0) ? n1 : n0;
        tex_s[0] = tex_s[1];
    }

    // The triangle list
    MYVERTEX* pVBVertices;
    int iVertex = 0;
    int iPair = 0;

    m_pVB->Lock( 0, 0, (BYTE**) &pVBVertices, 0 );
    for( i = 0; i < stacks; i++ )
    {
        for( j = 0; j < numPoints; j++, iPair++ )
        {
            if( j != 0 )
            {
                // the quad between this vertex pair and the last one
                pVBVertices[iVertex+0] = pVertices[2*iPair-1];
                pVBVertices[iVertex+1] = pVertices[2*iPair-2];
                pVBVertices[iVertex+2] = pVertices[2*iPair+1];
                pVBVertices[iVertex+3] = pVertices[2*iPair-2];
                pVBVertices[iVertex+4] = pVertices[2*iPair];
                pVBVertices[iVertex+5] = pVertices[2*iPair+1];
                iVertex += 6;
            }
        }
    }
    m_pVB->Unlock();

    SAFE_DELETE_ARRAY( pVertices );
}




//-----------------------------------------------------------------------------
// Name: PIPE_OBJECT::Build()
// Desc: A cylinder of the given length along +z (gluCylinder)
//-----------------------------------------------------------------------------
void PIPE_OBJECT::Build( OBJECT_BUILD_INFO* pBuildInfo, FLOAT length, FLOAT s_start, FLOAT s_end )
{
    int         stacks, slices;
    int         i, j;
    FLOAT       sinCache[CACHE_SIZE];
    FLOAT       cosCache[CACHE_SIZE];
    FLOAT       sinCache2[CACHE_SIZE];
    FLOAT       cosCache2[CACHE_SIZE];
    FLOAT       angle;
    FLOAT       zLow, zHigh;
    FLOAT       s_delta;
    FLOAT       radius = pBuildInfo->radius;
    IPOINT2D*   texRep = pBuildInfo->texRep;

    slices = pBuildInfo->nSlices;

    // About square quads
    stacks = (int) SS_ROUND_UP( (length / pBuildInfo->divSize) * (FLOAT) slices );

    if( slices >= CACHE_SIZE ) slices = CACHE_SIZE-1;
    if( stacks >= CACHE_SIZE ) stacks = CACHE_SIZE-1;

    s_delta = s_end - s_start;

    // Cache is the vertex locations cache, cache2 the normals
    for( i = 0; i < slices; i++ )
    {
        angle = 2 * PI * i / slices;
        sinCache2[i] = (FLOAT) sin( angle );
        cosCache2[i] = (FLOAT) cos( angle );
        sinCache[i] = (FLOAT) sin( angle );
        cosCache[i] = (FLOAT) cos( angle );
    }

    sinCache[slices] = sinCache[0];
    cosCache[slices] = cosCache[0];
    sinCache2[slices] = sinCache2[0];
    cosCache2[slices] = cosCache2[0];

    // Vertex buffer: 2 triangles per quad
    int numQuads = stacks * slices;
    m_dwNumTriangles = numQuads * 2;
    DWORD dwSize = numQuads * 6 * sizeof(MYVERTEX);

    if( FAILED( m_pd3dDevice->CreateVertexBuffer( dwSize, D3DUSAGE_WRITEONLY, D3DFVF_MYVERTEX,
                                                  D3DPOOL_MANAGED, &m_pVB ) ) )
        return;

    // The quad strips
    MYVERTEX* pVertices = new MYVERTEX[numQuads * 6];
    ZeroMemory( pVertices, dwSize );
    MYVERTEX* pVertex = pVertices;

    for( j = 0; j < stacks; j++ )
    {
        zLow = j * length / stacks;
        zHigh = (j + 1) * length / stacks;

        for( i = 0; i <= slices; i++ )
        {
            pVertex->p = D3DXVECTOR3( radius * sinCache[i], radius * cosCache[i], zLow );
            pVertex->n = D3DXVECTOR3( sinCache2[i], cosCache2[i], 0.0f );
            if( texRep )
            {
                pVertex->tu = (FLOAT) j * s_delta / stacks + s_start;
                pVertex->tv = (FLOAT) texRep->y * i / slices;
            }
            pVertex++;

            pVertex->p = D3DXVECTOR3( radius * sinCache[i], radius * cosCache[i], zHigh );
            pVertex->n = D3DXVECTOR3( sinCache2[i], cosCache2[i], 0.0f );
            if( texRep )
            {
                pVertex->tu = (FLOAT) (j + 1) * s_delta / stacks + s_start;
                pVertex->tv = (FLOAT) texRep->y * i / slices;
            }
            pVertex++;
        }
    }

    // The triangle list
    MYVERTEX* pVBVertices;
    int iVertex = 0;
    int iPair = 0;

    m_pVB->Lock( 0, 0, (BYTE**) &pVBVertices, 0 );
    for( j = 0; j < stacks; j++ )
    {
        for( i = 0; i <= slices; i++, iPair++ )
        {
            if( i != 0 )
            {
                // the quad between this vertex pair and the last one
                pVBVertices[iVertex+0] = pVertices[2*iPair-1];
                pVBVertices[iVertex+1] = pVertices[2*iPair-2];
                pVBVertices[iVertex+2] = pVertices[2*iPair+1];
                pVBVertices[iVertex+3] = pVertices[2*iPair-2];
                pVBVertices[iVertex+4] = pVertices[2*iPair];
                pVBVertices[iVertex+5] = pVertices[2*iPair+1];
                iVertex += 6;
            }
        }
    }
    m_pVB->Unlock();

    SAFE_DELETE_ARRAY( pVertices );
}




//-----------------------------------------------------------------------------
// Name: SPHERE_OBJECT::Build()
// Desc: A sphere around the origin, its poles along z (gluSphere)
//-----------------------------------------------------------------------------
void SPHERE_OBJECT::Build( OBJECT_BUILD_INFO* pBuildInfo, FLOAT radius, FLOAT s_start, FLOAT s_end )
{
    int         i, j;
    FLOAT       sinCache1a[CACHE_SIZE];
    FLOAT       cosCache1a[CACHE_SIZE];
    FLOAT       sinCache2a[CACHE_SIZE];
    FLOAT       cosCache2a[CACHE_SIZE];
    FLOAT       sinCache1b[CACHE_SIZE];
    FLOAT       cosCache1b[CACHE_SIZE];
    FLOAT       sinCache2b[CACHE_SIZE];
    FLOAT       cosCache2b[CACHE_SIZE];
    FLOAT       angle;
    FLOAT       zLow, zHigh;
    FLOAT       sintemp1, sintemp2, sintemp3, sintemp4;
    FLOAT       costemp3, costemp4;
    FLOAT       s_delta;
    int         stacks, slices;
    IPOINT2D*   texRep = pBuildInfo->texRep;

    slices = pBuildInfo->nSlices;
    if( slices >= CACHE_SIZE ) slices = CACHE_SIZE-1;
    stacks = slices;

    // s runs from s_end at the +z pole to s_start at the -z pole
    s_delta = s_start - s_end;

    // Cache 1 is the vertex locations cache, cache 2 the normals
    for( i = 0; i < slices; i++ )
    {
        angle = 2 * PI * i / slices;
        sinCache1a[i] = (FLOAT) sin( angle );
        cosCache1a[i] = (FLOAT) cos( angle );
        sinCache2a[i] = sinCache1a[i];
        cosCache2a[i] = cosCache1a[i];
    }

    for( j = 0; j <= stacks; j++ )
    {
        angle = PI * j / stacks;
        sinCache2b[j] = (FLOAT) sin( angle );
        cosCache2b[j] = (FLOAT) cos( angle );
        sinCache1b[j] = radius * (FLOAT) sin( angle );
        cosCache1b[j] = radius * (FLOAT) cos( angle );
    }

    // Make sure it comes to a point
    sinCache1b[0] = 0.0f;
    sinCache1b[stacks] = 0.0f;

    sinCache1a[slices] = sinCache1a[0];
    cosCache1a[slices] = cosCache1a[0];
    sinCache2a[slices] = sinCache2a[0];
    cosCache2a[slices] = cosCache2a[0];

    // Vertex buffer: 2 triangles per quad
    int numQuads = slices * stacks;
    m_dwNumTriangles = numQuads * 2;
    DWORD dwSize = numQuads * 6 * sizeof(MYVERTEX);

    if( FAILED( m_pd3dDevice->CreateVertexBuffer( dwSize, D3DUSAGE_WRITEONLY, D3DFVF_MYVERTEX,
                                                  D3DPOOL_MANAGED, &m_pVB ) ) )
        return;

    // The quad strips
    MYVERTEX* pVertices = new MYVERTEX[numQuads * 6];
    ZeroMemory( pVertices, dwSize );
    MYVERTEX* pVertex = pVertices;

    for( j = 0; j < stacks; j++ )
    {
        zLow = cosCache1b[j];
        zHigh = cosCache1b[j+1];
        sintemp1 = sinCache1b[j];
        sintemp2 = sinCache1b[j+1];
        sintemp3 = sinCache2b[j+1];
        costemp3 = cosCache2b[j+1];
        sintemp4 = sinCache2b[j];
        costemp4 = cosCache2b[j];

        for( i = 0; i <= slices; i++ )
        {
            pVertex->p = D3DXVECTOR3( sintemp2 * sinCache1a[i], sintemp2 * cosCache1a[i], zHigh );
            pVertex->n = D3DXVECTOR3( sintemp3 * sinCache2a[i], sintemp3 * cosCache2a[i], costemp3 );
            if( texRep )
            {
                pVertex->tu = (FLOAT) (j + 1) * s_delta / stacks + s_end;
                pVertex->tv = (FLOAT) texRep->y * i / slices;
            }
            pVertex++;

            pVertex->p = D3DXVECTOR3( sintemp1 * sinCache1a[i], sintemp1 * cosCache1a[i], zLow );
            pVertex->n = D3DXVECTOR3( sintemp4 * sinCache2a[i], sintemp4 * cosCache2a[i], costemp4 );
            if( texRep )
            {
                pVertex->tu = (FLOAT) j * s_delta / stacks + s_end;
                pVertex->tv = (FLOAT) texRep->y * i / slices;
            }
            pVertex++;
        }
    }

    // The triangle list
    MYVERTEX* pVBVertices;
    int iVertex = 0;
    int iPair = 0;

    m_pVB->Lock( 0, 0, (BYTE**) &pVBVertices, 0 );
    for( j = 0; j < stacks; j++ )
    {
        for( i = 0; i <= slices; i++, iPair++ )
        {
            if( i != 0 )
            {
                // the quad between this vertex pair and the last one
                pVBVertices[iVertex+0] = pVertices[2*iPair-1];
                pVBVertices[iVertex+1] = pVertices[2*iPair-2];
                pVBVertices[iVertex+2] = pVertices[2*iPair+1];
                pVBVertices[iVertex+3] = pVertices[2*iPair-2];
                pVBVertices[iVertex+4] = pVertices[2*iPair];
                pVBVertices[iVertex+5] = pVertices[2*iPair+1];
                iVertex += 6;
            }
        }
    }
    m_pVB->Unlock();

    SAFE_DELETE_ARRAY( pVertices );
}




//-----------------------------------------------------------------------------
// Name: PIPE_OBJECT()
// Desc:
//-----------------------------------------------------------------------------
PIPE_OBJECT::PIPE_OBJECT( IDirect3DDevice8* pd3dDevice, OBJECT_BUILD_INFO* pBuildInfo,
                          FLOAT length ) : OBJECT( pd3dDevice )
{
    Build( pBuildInfo, length, 0.0f, 0.0f );
}

PIPE_OBJECT::PIPE_OBJECT( IDirect3DDevice8* pd3dDevice, OBJECT_BUILD_INFO* pBuildInfo,
                          FLOAT length, FLOAT s_start, FLOAT s_end ) : OBJECT( pd3dDevice )
{
    Build( pBuildInfo, length, s_start, s_end );
}




//-----------------------------------------------------------------------------
// Name: BALLJOINT_OBJECT()
// Desc:
//-----------------------------------------------------------------------------
BALLJOINT_OBJECT::BALLJOINT_OBJECT( IDirect3DDevice8* pd3dDevice, OBJECT_BUILD_INFO* pBuildInfo,
                                    int notch, FLOAT s_start, FLOAT s_end ) : OBJECT( pd3dDevice )
{
    Build( pBuildInfo, notch, s_start, s_end );
}




//-----------------------------------------------------------------------------
// Name: SPHERE_OBJECT()
// Desc:
//-----------------------------------------------------------------------------
SPHERE_OBJECT::SPHERE_OBJECT( IDirect3DDevice8* pd3dDevice, OBJECT_BUILD_INFO* pBuildInfo,
                              FLOAT radius ) : OBJECT( pd3dDevice )
{
    Build( pBuildInfo, radius, 0.0f, 0.0f );
}

SPHERE_OBJECT::SPHERE_OBJECT( IDirect3DDevice8* pd3dDevice, OBJECT_BUILD_INFO* pBuildInfo,
                              FLOAT radius, FLOAT s_start, FLOAT s_end ) : OBJECT( pd3dDevice )
{
    Build( pBuildInfo, radius, s_start, s_end );
}




//-----------------------------------------------------------------------------
// Name: ELBOW_OBJECT::Build()
// Desc: A 90 degree elbow: a ring rotated in steps about the x axis.  The
//       4 elbows start at the 4 orientations of the notch.
//-----------------------------------------------------------------------------
void ELBOW_OBJECT::Build( OBJECT_BUILD_INFO* pBuildInfo, int notch, FLOAT s_start, FLOAT s_end )
{
    int         stacks, slices;
    FLOAT       angle, startAng;
    int         numPoints;
    FLOAT       s_delta;
    int         i, j;
    D3DXVECTOR3 pi[CACHE_SIZE];     // initial ring of points + center
    D3DXVECTOR3 p0[CACHE_SIZE];     // 2 rings of points
    D3DXVECTOR3 p1[CACHE_SIZE];
    D3DXVECTOR3 n0[CACHE_SIZE];     // 2 rings of normals
    D3DXVECTOR3 n1[CACHE_SIZE];
    FLOAT       tex_t[CACHE_SIZE];  // t texture coordinates
    FLOAT       tex_s[2];           // s texture coordinates
    D3DXVECTOR3 center;             // center of the ring
    D3DXVECTOR3 rotCenter;          // center of the rotations
    D3DXVECTOR3 *pPoints0, *pPoints1, *pNormals0, *pNormals1;
    FLOAT       radius = pBuildInfo->radius;
    IPOINT2D*   texRep = pBuildInfo->texRep;

    slices = pBuildInfo->nSlices;
    stacks = slices / 2;

    if( slices >= CACHE_SIZE ) slices = CACHE_SIZE-1;
    if( stacks >= CACHE_SIZE ) stacks = CACHE_SIZE-1;

    s_delta = s_end - s_start;

    // t texture coordinates
    if( texRep )
    {
        for( i = 0; i <= slices; i++ )
            tex_t[i] = (FLOAT) i * texRep->y / slices;
    }

    numPoints = slices + 1;

    // The initial ring: a circle in the xz plane around (0, r, -r),
    // starting at the notch
    startAng = notch * PI / 2;
    for( i = 0; i <= slices; i++ )
    {
        angle = (2 * PI * i / slices) + startAng;
        pi[i].x = radius * (FLOAT) sin( angle );
        pi[i].y = radius;
        pi[i].z = radius * (FLOAT) cos( angle ) - radius;
    }

    // and its center
    pi[i].x = 0.0f;
    pi[i].y = radius;
    pi[i].z = -radius;
    center = pi[i];

    // The rings are rotated about the x axis through (0, r, 0)
    rotCenter = D3DXVECTOR3( 0.0f, radius, 0.0f );

    CalcNormals( pi, n0, &center, numPoints );

    tex_s[0] = s_start;
    pPoints0 = pi;
    pPoints1 = p1;
    pNormals0 = n0;
    pNormals1 = n1;

    // Vertex buffer: 2 triangles per quad
    int numQuads = (numPoints - 1) * stacks;
    m_dwNumTriangles = numQuads * 2;
    DWORD dwSize = numQuads * 6 * sizeof(MYVERTEX);

    if( FAILED( m_pd3dDevice->CreateVertexBuffer( dwSize, D3DUSAGE_WRITEONLY, D3DFVF_MYVERTEX,
                                                  D3DPOOL_MANAGED, &m_pVB ) ) )
        return;

    // The quad strips
    MYVERTEX* pVertices = new MYVERTEX[numQuads * 6];
    ZeroMemory( pVertices, dwSize );
    MYVERTEX* pVertex = pVertices;

    for( i = 1; i <= stacks; i++ )
    {
        // The initial ring and its center, rotated into place
        TransformCircle( (FLOAT) i * -PI_OVER_2 / stacks, pi, pPoints1, numPoints + 1, &rotCenter );

        center = pPoints1[numPoints];
        CalcNormals( pPoints1, pNormals1, &center, numPoints );

        tex_s[1] = (FLOAT) i * s_delta / stacks + s_start;

        // A quad strip between the last ring and this one
        for( j = 0; j < numPoints; j++ )
        {
            pVertex->p = pPoints0[j];
            pVertex->n = pNormals0[j];
            if( texRep )
            {
                pVertex->tu = tex_s[0];
                pVertex->tv = tex_t[j];
            }
            pVertex++;

            pVertex->p = pPoints1[j];
            pVertex->n = pNormals1[j];
            if( texRep )
            {
                pVertex->tu = tex_s[1];
                pVertex->tv = tex_t[j];
            }
            pVertex++;
        }

        // Swap the buffers
        pPoints0 = pPoints1;
        pPoints1 = (pPoints1 == p1) ? p0 : p1;
        pNormals0 = pNormals1;
        pNormals1 = (pNormals1 == n0) ? n1 : n0;
        tex_s[0] = tex_s[1];
    }

    // The triangle list
    MYVERTEX* pVBVertices;
    int iVertex = 0;
    int iPair = 0;

    m_pVB->Lock( 0, 0, (BYTE**) &pVBVertices, 0 );
    for( i = 0; i < stacks; i++ )
    {
        for( j = 0; j < numPoints; j++, iPair++ )
        {
            if( j != 0 )
            {
                // the quad between this vertex pair and the last one
                pVBVertices[iVertex+0] = pVertices[2*iPair-1];
                pVBVertices[iVertex+1] = pVertices[2*iPair-2];
                pVBVertices[iVertex+2] = pVertices[2*iPair+1];
                pVBVertices[iVertex+3] = pVertices[2*iPair-2];
                pVBVertices[iVertex+4] = pVertices[2*iPair];
                pVBVertices[iVertex+5] = pVertices[2*iPair+1];
                iVertex += 6;
            }
        }
    }
    m_pVB->Unlock();

    SAFE_DELETE_ARRAY( pVertices );
}




//-----------------------------------------------------------------------------
// Name: ELBOW_OBJECT()
// Desc:
//-----------------------------------------------------------------------------
ELBOW_OBJECT::ELBOW_OBJECT( IDirect3DDevice8* pd3dDevice, OBJECT_BUILD_INFO* pBuildInfo,
                            int notch ) : OBJECT( pd3dDevice )
{
    Build( pBuildInfo, notch, 0.0f, 0.0f );
}

ELBOW_OBJECT::ELBOW_OBJECT( IDirect3DDevice8* pd3dDevice, OBJECT_BUILD_INFO* pBuildInfo,
                            int notch, FLOAT s_start, FLOAT s_end ) : OBJECT( pd3dDevice )
{
    Build( pBuildInfo, notch, s_start, s_end );
}
