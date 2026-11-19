//-----------------------------------------------------------------------------
// File: explode.cpp
//
// Desc: The "Explode" style: a sphere bursts into its faces, which spin
//       away and come back together.
//
//       (explode.c of the OpenGL version, ported to Direct3D 8)
//-----------------------------------------------------------------------------
#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include "ss3dfo.h"

#define MAX_PREC        20
#define EXPLODE_FRAMES  30      // steps of a burst

// The 4 points of a face (the interleaved N3F_V3F layout of the OpenGL
// version)
struct NVERTEX
{
    D3DXVECTOR3 n;
    D3DXVECTOR3 p;
};


static MESH     explodeMesh;    // the sphere
static FLOAT*   xrot;           // rotation of each face
static FLOAT*   yrot;
static FLOAT*   zrot;
static MATRIX*  faceMatrices;   // transform of each face
static FLOAT*   xrotInc;
static FLOAT*   yrotInc;
static FLOAT*   zrotInc;
static NVERTEX* faceVerts;      // the transformed faces
static FLOAT    fTime;          // time of the burst
static int      iFrame;         // EXPLODE_FRAMES steps
static FLOAT    fLastScale;
static FLOAT    fScaleTime;

static FLOAT    fScale;         // how far the faces are
static FLOAT    fWait;          // time to wait before the burst
static D3DXCOLOR color;
static FLOAT    hue;

static int      iPrec = 10;
static FLOAT    matlColor[4] = { 1.0f, 0.8f, 0.0f, 1.0f };
static FLOAT    specColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static BOOL     bExplode = TRUE;
static FLOAT    fScaleCount = 1.0f;




//-----------------------------------------------------------------------------
// Name: initFaces()
// Desc: Random rotation speeds for the faces
//-----------------------------------------------------------------------------
static void initFaces()
{
    int i;

    for( i = 0; i < explodeMesh.numFaces; i++ )
    {
        matrixIdent( &faceMatrices[i] );
        xrotInc[i] = (FLOAT) (rand() & 0x3) * PI / 31.0f;
        yrotInc[i] = (FLOAT) (rand() & 0x3) * PI / 31.0f;
        zrotInc[i] = (FLOAT) (rand() & 0x3) * PI / 31.0f;
        xrot[i] = 0.0f;
        yrot[i] = 0.0f;
        zrot[i] = 0.0f;
    }
}




//-----------------------------------------------------------------------------
// Name: genExplode()
// Desc: The sphere
//-----------------------------------------------------------------------------
static void genExplode()
{
    int i;
    double angle;
    D3DXVECTOR3 circle[MAX_PREC + 1];

    angle = PI / 2;
    for( i = 0; i < iPrec; i++ )
    {
        circle[i].x = (FLOAT) (cos( angle ) * 0.3);
        circle[i].y = (FLOAT) (sin( angle ) * 0.3);
        circle[i].z = 0.0f;
        angle += -PI / (iPrec - 1);
    }

    revolveSurface( &explodeMesh, circle, iPrec );

    initFaces();
}




//-----------------------------------------------------------------------------
// Name: initExplodeScene()
// Desc:
//-----------------------------------------------------------------------------
BOOL initExplodeScene()
{
    D3DXMATRIX matView;

    iPrec = (int) (fTesselFact * 10.5);
    if( iPrec < 5 )
        iPrec = 5;
    if( iPrec > MAX_PREC )
        iPrec = MAX_PREC;

    faceMatrices = (MATRIX*) SaverAlloc( iPrec * iPrec * sizeof(MATRIX) );
    if( !faceMatrices )
        return FALSE;
    xrotInc = (FLOAT*) SaverAlloc( iPrec * iPrec * sizeof(FLOAT) );
    if( !xrotInc )
        return FALSE;
    yrotInc = (FLOAT*) SaverAlloc( iPrec * iPrec * sizeof(FLOAT) );
    if( !yrotInc )
        return FALSE;
    zrotInc = (FLOAT*) SaverAlloc( iPrec * iPrec * sizeof(FLOAT) );
    if( !zrotInc )
        return FALSE;
    xrot = (FLOAT*) SaverAlloc( iPrec * iPrec * sizeof(FLOAT) );
    if( !xrot )
        return FALSE;
    yrot = (FLOAT*) SaverAlloc( iPrec * iPrec * sizeof(FLOAT) );
    if( !yrot )
        return FALSE;
    zrot = (FLOAT*) SaverAlloc( iPrec * iPrec * sizeof(FLOAT) );
    if( !zrot )
        return FALSE;

    genExplode();

    faceVerts = (NVERTEX*) SaverAlloc( explodeMesh.numFaces * 4 * sizeof(NVERTEX) );

    myglMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, matlColor );
    myglMaterialfv( GL_FRONT, GL_SPECULAR, specColor );
    myglMaterialf( GL_FRONT, GL_SHININESS, 100.0f );

    SetProjectionParams( FALSE, 0.66f, 0.66f, 0.3f, 3.0f );

    D3DXVECTOR3 vEye( 0.0f, 0.0f, 1.5f );
    D3DXVECTOR3 vAt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtLH( &matView, &vEye, &vAt, &vUp );
    g_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: delExplodeScene()
// Desc:
//-----------------------------------------------------------------------------
void delExplodeScene()
{
    delMesh( &explodeMesh );

    SaverFree( faceMatrices );
    SaverFree( xrotInc );
    SaverFree( yrotInc );
    SaverFree( zrotInc );
    SaverFree( xrot );
    SaverFree( yrot );
    SaverFree( zrot );
    SaverFree( faceVerts );
}




//-----------------------------------------------------------------------------
// Name: updateExplodeScene()
// Desc:
//-----------------------------------------------------------------------------
void updateExplodeScene( int flags, FLOAT fElapsedTime )
{
    int i, k;
    FLOAT t;
    NVERTEX* pFace;
    FACE* face;

    if( fElapsedTime > 0.25f )
        fElapsedTime = 0.25f;
    t = fElapsedTime * 20.0f;

    // (without color cycling the first color stays)
    if( bColorCycle || hue == 0.0f )
    {
        HsvToRgb( hue, 1.0f, 1.0f, &color );
        myglMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, (FLOAT*) &color );

        hue += t;
        if( hue >= 360.0f )
            hue -= 360.0f;
    }

    // Move the faces: each one turns about its first point and moves away
    // from the centre
    for( i = 0, face = explodeMesh.faces, pFace = faceVerts;
         i < explodeMesh.numFaces;
         i++, face++, pFace += 4 )
    {
        MATRIX* mat = &faceMatrices[i];
        D3DXVECTOR3 p0;

        matrixIdent( mat );
        matrixRotate( mat, xrot[i], yrot[i], zrot[i] );

        if( fWait <= 0.0f )
        {
            xrot[i] += t * xrotInc[i];
            yrot[i] += t * yrotInc[i];
            zrot[i] += t * zrotInc[i];
        }

        // (the quad order of the OpenGL version)
        pFace[0].p = explodeMesh.pts[face->p[0]];
        pFace[1].p = explodeMesh.pts[face->p[1]];
        pFace[2].p = explodeMesh.pts[face->p[3]];
        pFace[3].p = explodeMesh.pts[face->p[2]];

        p0 = pFace[0].p;
        for( k = 0; k < 4; k++ )
        {
            pFace[k].p -= p0;
            xformPoint( &pFace[k].p, &pFace[k].p, mat );
            pFace[k].p.x += p0.x + p0.x * fScale;
            pFace[k].p.y += p0.y + p0.y * fScale;
            pFace[k].p.z += p0.z + p0.z * fScale;
        }

        if( !bSmoothShading )
        {
            pFace[0].n = face->norm;
            xformNorm( &pFace[0].n, &pFace[0].n, mat );
            pFace[1].n = pFace[0].n;
            pFace[2].n = pFace[0].n;
            pFace[3].n = pFace[0].n;
        }
        else
        {
            pFace[0].n = explodeMesh.norms[face->p[0]];
            pFace[1].n = explodeMesh.norms[face->p[1]];
            pFace[2].n = explodeMesh.norms[face->p[3]];
            pFace[3].n = explodeMesh.norms[face->p[2]];
            for( k = 0; k < 4; k++ )
                xformNorm( &pFace[k].n, &pFace[k].n, mat );
        }
    }

    // Draw them
    g_pd3dDevice->SetVertexShader( D3DFVF_MESHVERTEX );
    {
        static MESHVERTEX vertices[5000];
        static WORD indices[5000];
        int numVerts = 0;
        int numPrims = 0;
        MESHVERTEX* pVertex = vertices;
        WORD* pIndex = indices;

        if( explodeMesh.numFaces > 0 )
        {
            numPrims = explodeMesh.numFaces * 2;
            for( i = 0, pFace = faceVerts; i < explodeMesh.numFaces; i++, pFace += 4 )
            {
                for( k = 0; k < 4; k++ )
                {
                    pVertex[k].p = pFace[k].p;
                    pVertex[k].n = pFace[k].n;
                }

                pIndex[0] = (WORD) (numVerts);
                pIndex[1] = (WORD) (numVerts + 1);
                pIndex[2] = (WORD) (numVerts + 2);
                pIndex[3] = (WORD) (numVerts);
                pIndex[4] = (WORD) (numVerts + 2);
                pIndex[5] = (WORD) (numVerts + 3);

                numVerts += 4;
                pVertex += 4;
                pIndex += 6;
            }
        }

        g_pd3dDevice->DrawIndexedPrimitiveUP( D3DPT_TRIANGLELIST, 0, numVerts, numPrims,
                                              indices, D3DFMT_INDEX16, vertices,
                                              sizeof(MESHVERTEX) );
    }

    if( fWait <= 0.0f )
    {
        // The burst: fast at first, then slower; then it comes back
        int iTarget;

        fScaleTime += t;
        iTarget = (int) fScaleTime;
        while( (int) fScaleCount < iTarget )
        {
            fScaleCount += 1.0;
            if( bExplode )
            {
                fLastScale = fScale;
                fScale += (FLOAT) (pow( (EXPLODE_FRAMES - iFrame) / 30.0, 4.0 ) * 0.3);
            }
            else
            {
                fScale -= fLastScale / 30.0;
            }
        }

        fTime += t;
        iFrame = (int) fTime;
        if( iFrame > EXPLODE_FRAMES )
        {
            bExplode ^= 1;
            fTime = 0.0f;
            iFrame = 0;

            if( bExplode == TRUE )
            {
                fScale = 0.0f;
                fWait = 10.0f;
                fScaleTime = 0.0f;
                fScaleCount = 1.0f;
                initFaces();
            }
        }
    }
    else
    {
        fWait -= t;
    }
}
