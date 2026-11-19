//-----------------------------------------------------------------------------
// File: mesh.cpp
//
// Desc: Meshes of quads: building them (revolved surfaces), the point and
//       normal helpers, and drawing them with the shared vertex and index
//       buffers of the device.
//
//       (mesh.c of the OpenGL version, ported to Direct3D 8)
//-----------------------------------------------------------------------------
#include <windows.h>
#include <math.h>
#include "ss3dfo.h"

#define ZERO_EPS    0.00000001

// The origin (zero normal)
D3DXVECTOR3 ptZero;




//-----------------------------------------------------------------------------
// Name: xformPoint()
// Desc: ptOut = mat * ptIn
//-----------------------------------------------------------------------------
void xformPoint( D3DXVECTOR3* ptOut, D3DXVECTOR3* ptIn, MATRIX* mat )
{
    double x, y, z;

    x = (ptIn->x * mat->M[0][0]) + (ptIn->y * mat->M[0][1]) +
        (ptIn->z * mat->M[0][2]) + mat->M[0][3];

    y = (ptIn->x * mat->M[1][0]) + (ptIn->y * mat->M[1][1]) +
        (ptIn->z * mat->M[1][2]) + mat->M[1][3];

    z = (ptIn->x * mat->M[2][0]) + (ptIn->y * mat->M[2][1]) +
        (ptIn->z * mat->M[2][2]) + mat->M[2][3];

    ptOut->x = (FLOAT) x;
    ptOut->y = (FLOAT) y;
    ptOut->z = (FLOAT) z;
}




//-----------------------------------------------------------------------------
// Name: xformNorm()
// Desc: ptOut = normalized rotation of ptIn
//-----------------------------------------------------------------------------
void xformNorm( D3DXVECTOR3* ptOut, D3DXVECTOR3* ptIn, MATRIX* mat )
{
    FLOAT x, y, z;
    FLOAT len;

    x = (ptIn->x * mat->M[0][0]) + (ptIn->y * mat->M[0][1]) +
        (ptIn->z * mat->M[0][2]);

    y = (ptIn->x * mat->M[1][0]) + (ptIn->y * mat->M[1][1]) +
        (ptIn->z * mat->M[1][2]);

    z = (ptIn->x * mat->M[2][0]) + (ptIn->y * mat->M[2][1]) +
        (ptIn->z * mat->M[2][2]);

    len = (x * x) + (y * y) + (z * z);
    if( len >= ZERO_EPS )
        len = 1.0f / (FLOAT) sqrt( len );
    else
        len = 1.0f;

    ptOut->x = x * len;
    ptOut->y = y * len;
    ptOut->z = z * len;
}




//-----------------------------------------------------------------------------
// Name: matrixIdent()
// Desc:
//-----------------------------------------------------------------------------
void matrixIdent( MATRIX* mat )
{
    mat->M[0][0] = 1.0f; mat->M[0][1] = 0.0f; mat->M[0][2] = 0.0f; mat->M[0][3] = 0.0f;
    mat->M[1][0] = 0.0f; mat->M[1][1] = 1.0f; mat->M[1][2] = 0.0f; mat->M[1][3] = 0.0f;
    mat->M[2][0] = 0.0f; mat->M[2][1] = 0.0f; mat->M[2][2] = 1.0f; mat->M[2][3] = 0.0f;
    mat->M[3][0] = 0.0f; mat->M[3][1] = 0.0f; mat->M[3][2] = 0.0f; mat->M[3][3] = 1.0f;
}




//-----------------------------------------------------------------------------
// Name: matrixRotate()
// Desc: Sets the rotation of a matrix, keeping its scale
//-----------------------------------------------------------------------------
void matrixRotate( MATRIX* m, double xTheta, double yTheta, double zTheta )
{
    FLOAT xScale, yScale, zScale;
    FLOAT sinX, cosX;
    FLOAT sinY, cosY;
    FLOAT sinZ, cosZ;

    xScale = m->M[0][0];
    yScale = m->M[1][1];
    zScale = m->M[2][2];
    sinX = (FLOAT) sin( xTheta );
    cosX = (FLOAT) cos( xTheta );
    sinY = (FLOAT) sin( yTheta );
    cosY = (FLOAT) cos( yTheta );
    sinZ = (FLOAT) sin( zTheta );
    cosZ = (FLOAT) cos( zTheta );

    m->M[0][0] = (FLOAT) ((cosZ * cosY) * xScale);
    m->M[0][1] = (FLOAT) ((cosZ * -sinY * -sinX + sinZ * cosX) * yScale);
    m->M[0][2] = (FLOAT) ((cosZ * -sinY * cosX + sinZ * sinX) * zScale);

    m->M[1][0] = (FLOAT) (-sinZ * cosY * xScale);
    m->M[1][1] = (FLOAT) ((-sinZ * -sinY * -sinX + cosZ * cosX) * yScale);
    m->M[1][2] = (FLOAT) ((-sinZ * -sinY * cosX + cosZ * sinX) * zScale);

    m->M[2][0] = (FLOAT) (sinY * xScale);
    m->M[2][1] = (FLOAT) (cosY * -sinX * yScale);
    m->M[2][2] = (FLOAT) (cosY * cosX * zScale);
}




//-----------------------------------------------------------------------------
// Name: matrixTranslate()
// Desc: Sets the translation of a matrix
//-----------------------------------------------------------------------------
void matrixTranslate( MATRIX* m, double xTrans, double yTrans, double zTrans )
{
    m->M[0][3] = (FLOAT) xTrans;
    m->M[1][3] = (FLOAT) yTrans;
    m->M[2][3] = (FLOAT) zTrans;
}




//-----------------------------------------------------------------------------
// Name: calcNorm()
// Desc: The normal of the plane through p1, p2 and p3:
//       (p2 - p1) x (p3 - p1), normalized
//-----------------------------------------------------------------------------
void calcNorm( D3DXVECTOR3* norm, D3DXVECTOR3* p1, D3DXVECTOR3* p2, D3DXVECTOR3* p3 )
{
    FLOAT crossX, crossY, crossZ;
    FLOAT abX, abY, abZ;
    FLOAT acX, acY, acZ;
    FLOAT sqrLength;
    FLOAT invLength;

    abX = p2->x - p1->x;
    abY = p2->y - p1->y;
    abZ = p2->z - p1->z;

    acX = p3->x - p1->x;
    acY = p3->y - p1->y;
    acZ = p3->z - p1->z;

    crossX = (abY * acZ) - (abZ * acY);
    crossY = (abZ * acX) - (abX * acZ);
    crossZ = (abX * acY) - (abY * acX);

    sqrLength = (crossX * crossX) + (crossY * crossY) + (crossZ * crossZ);

    if( sqrLength > ZERO_EPS )
        invLength = 1.0f / (FLOAT) sqrt( sqrLength );
    else
        invLength = 1.0f;

    norm->x = crossX * invLength;
    norm->y = crossY * invLength;
    norm->z = crossZ * invLength;
}




//-----------------------------------------------------------------------------
// Name: normalizeNorms()
// Desc:
//-----------------------------------------------------------------------------
void normalizeNorms( D3DXVECTOR3* p, ULONG cPts )
{
    FLOAT len;
    ULONG i;

    for( i = 0; i < cPts; i++, p++ )
    {
        len = (p->x * p->x) + (p->y * p->y) + (p->z * p->z);
        if( len > ZERO_EPS )
            len = 1.0f / (FLOAT) sqrt( len );
        else
            len = 1.0f;

        p->x *= len;
        p->y *= len;
        p->z *= len;
    }
}




//-----------------------------------------------------------------------------
// Name: newMesh()
// Desc: Allocates the points and faces of a mesh
//-----------------------------------------------------------------------------
BOOL newMesh( MESH* mesh, int numFaces, int numPts )
{
    mesh->numFaces = 0;
    mesh->numPoints = 0;

    if( numPts )
    {
        mesh->pts = (D3DXVECTOR3*) SaverAlloc( (LONG) numPts * (LONG) sizeof(D3DXVECTOR3) );
        if( !mesh->pts )
            return FALSE;

        mesh->norms = (D3DXVECTOR3*) SaverAlloc( (LONG) numPts * (LONG) sizeof(D3DXVECTOR3) );
        if( !mesh->norms )
            return FALSE;
    }

    mesh->faces = (FACE*) SaverAlloc( (LONG) numFaces * (LONG) sizeof(FACE) );
    if( !mesh->faces )
        return FALSE;

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: delMesh()
// Desc:
//-----------------------------------------------------------------------------
void delMesh( MESH* mesh )
{
    SaverFree( mesh->pts );
    SaverFree( mesh->norms );
    SaverFree( mesh->faces );
}




//-----------------------------------------------------------------------------
// Name: findElem()
// Desc: The index of a point of the mesh (from iStart on), adding it if it
//       is not there yet.  The normal is added to the normal of the point.
//-----------------------------------------------------------------------------
static int findElem( MESH* mesh, int iStart, D3DXVECTOR3* pt, D3DXVECTOR3* norm )
{
    int i;
    D3DXVECTOR3* pts = mesh->pts + iStart;

    for( i = iStart; i < mesh->numPoints; i++, pts++ )
    {
        if( (fabs( pts->x - pt->x ) < ZERO_EPS) &&
            (fabs( pts->y - pt->y ) < ZERO_EPS) &&
            (fabs( pts->z - pt->z ) < ZERO_EPS) )
        {
            mesh->norms[i].x += norm->x;
            mesh->norms[i].y += norm->y;
            mesh->norms[i].z += norm->z;
            return i;
        }
    }

    mesh->pts[i] = *pt;
    mesh->norms[i] = *norm;
    mesh->numPoints++;

    return i;
}




//-----------------------------------------------------------------------------
// Name: revolveSurface()
// Desc: The surface of revolution of a curve about the y axis
//-----------------------------------------------------------------------------
void revolveSurface( MESH* triMesh, D3DXVECTOR3* curve, int steps )
{
    int i;
    int j;
    int facecount = 0;
    double rotation = 0.0;
    double rotInc;
    double cosVal;
    double sinVal;
    int stepsSqr;
    D3DXVECTOR3 norm;
    D3DXVECTOR3 a[MAXPREC];
    D3DXVECTOR3 b[MAXPREC];

    if( steps > MAXPREC )
        steps = MAXPREC;

    rotInc = (2.0 * PI) / (double)(steps - 1);
    stepsSqr = steps * steps;
    newMesh( triMesh, stepsSqr, 4 * stepsSqr );

    for( j = 0; j < steps; j++, rotation += rotInc )
    {
        cosVal = cos( rotation );
        sinVal = sin( rotation );
        for( i = 0; i < steps; i++ )
        {
            a[i].x = (FLOAT) (curve[i].x * cosVal + curve[i].z * sinVal);
            a[i].y = (FLOAT) (curve[i].y);
            a[i].z = (FLOAT) (curve[i].z * cosVal - curve[i].x * sinVal);
        }

        cosVal = cos( rotation + rotInc );
        sinVal = sin( rotation + rotInc );
        for( i = 0; i < steps; i++ )
        {
            b[i].x = (FLOAT) (curve[i].x * cosVal + curve[i].z * sinVal);
            b[i].y = (FLOAT) (curve[i].y);
            b[i].z = (FLOAT) (curve[i].z * cosVal - curve[i].x * sinVal);
        }

        for( i = 0; i < (steps - 1); i++ )
        {
            calcNorm( &norm, &b[i + 1], &b[i], &a[i] );
            if( (norm.x * norm.x) + (norm.y * norm.y) + (norm.z * norm.z) < 0.9 )
                calcNorm( &norm, &a[i], &a[i + 1], &b[i + 1] );

            triMesh->faces[facecount].material = j & 7;
            triMesh->faces[facecount].norm = norm;
            triMesh->faces[facecount].p[0] = findElem( triMesh, 0, &b[i], &norm );
            triMesh->faces[facecount].p[1] = findElem( triMesh, 0, &a[i], &norm );
            triMesh->faces[facecount].p[2] = findElem( triMesh, 0, &b[i + 1], &norm );
            triMesh->faces[facecount].p[3] = findElem( triMesh, 0, &a[i + 1], &norm );
            triMesh->numFaces++;
            facecount++;
        }
    }

    normalizeNorms( triMesh->norms, triMesh->numPoints );
}




//-----------------------------------------------------------------------------
// Name: updateObject()
// Desc: Draws a mesh: each quad is two triangles.  With smooth shading the
//       points have their own normals, else the normal of the face.
//-----------------------------------------------------------------------------
void updateObject( MESH* mesh, BOOL bSmooth )
{
    int i;
    int numPrims = 0;
    int numVerts = 0;
    FACE* faces;
    MESHVERTEX* pVertices;
    WORD* pIndices;

    g_pd3dDevice->SetVertexShader( D3DFVF_MESHVERTEX );

    g_pVB->Lock( 0, 0, (BYTE**) &pVertices, 0 );
    g_pIB->Lock( 0, 5000, (BYTE**) &pIndices, 0 );

    for( i = 0, faces = mesh->faces; i < mesh->numFaces; i++, faces++ )
    {
        pVertices[0].p = mesh->pts[faces->p[0]];
        pVertices[0].n = *(bSmooth ? &mesh->norms[faces->p[0]] : &faces->norm);
        pVertices[1].p = mesh->pts[faces->p[1]];
        pVertices[1].n = *(bSmooth ? &mesh->norms[faces->p[1]] : &faces->norm);
        pVertices[2].p = mesh->pts[faces->p[2]];
        pVertices[2].n = *(bSmooth ? &mesh->norms[faces->p[2]] : &faces->norm);
        pVertices[3].p = mesh->pts[faces->p[3]];
        pVertices[3].n = *(bSmooth ? &mesh->norms[faces->p[3]] : &faces->norm);

        pIndices[0] = (WORD) (numVerts);
        pIndices[1] = (WORD) (numVerts + 1);
        pIndices[2] = (WORD) (numVerts + 2);
        pIndices[3] = (WORD) (numVerts + 2);
        pIndices[4] = (WORD) (numVerts + 1);
        pIndices[5] = (WORD) (numVerts + 3);

        numVerts += 4;
        numPrims += 2;
        pVertices += 4;
        pIndices += 6;
    }

    g_pVB->Unlock();
    g_pIB->Unlock();

    g_pd3dDevice->SetStreamSource( 0, g_pVB, sizeof(MESHVERTEX) );
    g_pd3dDevice->SetIndices( g_pIB, 0 );
    g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, numVerts, 0, numPrims );
}




//-----------------------------------------------------------------------------
// Name: updateObjectBack()
// Desc: Draws the back faces of a mesh (normals turned around, reversed
//       winding).  The OpenGL version lit both sides of the surfaces.
//-----------------------------------------------------------------------------
void updateObjectBack( MESH* mesh, BOOL bSmooth )
{
    int i;
    int numPrims = 0;
    int numVerts = 0;
    FACE* faces;
    MESHVERTEX* pVertices;
    WORD* pIndices;

    g_pd3dDevice->SetVertexShader( D3DFVF_MESHVERTEX );

    g_pVB->Lock( 0, 0, (BYTE**) &pVertices, 0 );
    g_pIB->Lock( 0, 5000, (BYTE**) &pIndices, 0 );

    for( i = 0, faces = mesh->faces; i < mesh->numFaces; i++, faces++ )
    {
        pVertices[0].p = mesh->pts[faces->p[0]];
        pVertices[0].n = bSmooth ? -mesh->norms[faces->p[0]] : -faces->norm;
        pVertices[1].p = mesh->pts[faces->p[1]];
        pVertices[1].n = bSmooth ? -mesh->norms[faces->p[1]] : -faces->norm;
        pVertices[2].p = mesh->pts[faces->p[2]];
        pVertices[2].n = bSmooth ? -mesh->norms[faces->p[2]] : -faces->norm;
        pVertices[3].p = mesh->pts[faces->p[3]];
        pVertices[3].n = bSmooth ? -mesh->norms[faces->p[3]] : -faces->norm;

        pIndices[0] = (WORD) (numVerts + 1);
        pIndices[1] = (WORD) (numVerts);
        pIndices[2] = (WORD) (numVerts + 2);
        pIndices[3] = (WORD) (numVerts + 1);
        pIndices[4] = (WORD) (numVerts + 2);
        pIndices[5] = (WORD) (numVerts + 3);

        numVerts += 4;
        numPrims += 2;
        pVertices += 4;
        pIndices += 6;
    }

    g_pVB->Unlock();
    g_pIB->Unlock();

    g_pd3dDevice->SetStreamSource( 0, g_pVB, sizeof(MESHVERTEX) );
    g_pd3dDevice->SetIndices( g_pIB, 0 );
    g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, numVerts, 0, numPrims );
}
