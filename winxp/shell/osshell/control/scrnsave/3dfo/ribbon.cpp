//-----------------------------------------------------------------------------
// File: ribbon.cpp
//
// Desc: The "Ribbon" and "2 Ribbons" styles: a ribbon along a Hermite curve
//       whose end and tangent keep moving.  It changes the axis it spins
//       about whenever its window bounces.
//
//       (ribbon.c of the OpenGL version, ported to Direct3D 8)
//-----------------------------------------------------------------------------
#include <windows.h>
#include <math.h>
#include "ss3dfo.h"


static MESH     ribbonMesh;
static FLOAT    ribbonRot;      // (not used)
static FLOAT    theta;          // the tangent at the start of the curve
static int      iFrame;
static FLOAT    hue;
static double   zrot;
static double   yrot;
static double   xrot;
static double   zrotInc;
static double   xrotInc;
static BOOL     bBounced;       // the window bounced last frame

static int      iPrec = 40;
static FLOAT    hermiteMatrix[4][4] =
{
    {  2.0f, -2.0f,  1.0f,  1.0f },
    { -3.0f,  3.0f, -2.0f, -1.0f },
    {  0.0f,  0.0f,  1.0f,  0.0f },
    {  1.0f,  0.0f,  0.0f,  0.0f },
};
static FLOAT    phi = PI / 2;   // the end of the curve
static double   yrotInc = 0.1;




//-----------------------------------------------------------------------------
// Name: genRibbon()
// Desc: The ribbon: iPrec points along the curve, at y = 0.25 and -0.25
//-----------------------------------------------------------------------------
static void genRibbon( FLOAT fStep )
{
    int i;
    FLOAT t, t2, t3;
    FLOAT x, z;
    FLOAT gx[4], gy[4], gz[4];      // geometry: 2 points, 2 tangents
    FLOAT cx[4], cy[4], cz[4];      // coefficients of the curve
    D3DXVECTOR3 norm;
    int numPts = iPrec * 2;

    iFrame++;

    // The geometry of the curve
    gx[0] = -0.5f;
    gx[1] = (FLOAT) (sin( phi ) * 0.5);
    gx[2] = (FLOAT) (cos( theta ) * 4.0);
    gx[3] = 0.0f;
    gy[0] = gy[1] = gy[2] = gy[3] = 0.0f;
    gz[0] = 0.0f;
    gz[1] = 0.0f;
    gz[2] = (FLOAT) (sin( theta ) * 4.0);
    gz[3] = 3.0f;

    theta += (FLOAT) ((PI / (2.0 * iPrec)) * fStep);
    phi += (FLOAT) ((PI / (4.0 * iPrec)) * fStep);

    if( iFrame >= numPts )
        iFrame = 0;

    ribbonRot += 0.03926991f;

    // The coefficients
    for( i = 0; i < 4; i++ )
    {
        cx[i] = hermiteMatrix[i][1] * gx[1] + hermiteMatrix[i][2] * gx[2] +
                hermiteMatrix[i][0] * gx[0] + hermiteMatrix[i][3] * gx[3];
        cz[i] = hermiteMatrix[i][3] * gz[3] + hermiteMatrix[i][2] * gz[2] +
                hermiteMatrix[i][0] * gz[0] + hermiteMatrix[i][1] * gz[1];
        cy[i] = hermiteMatrix[i][2] * gy[2] + hermiteMatrix[i][0] * gy[0] +
                hermiteMatrix[i][1] * gy[1] + hermiteMatrix[i][3] * gy[3];
    }

    // The points
    t = 0.0f;
    for( i = 0; i < iPrec; i++ )
    {
        t += 1.0f / (FLOAT) iPrec;
        t2 = t * t;
        t3 = t2 * t;

        x = cx[2] * t + cx[1] * t2 + cx[0] * t3 + cx[3];
        z = cz[2] * t + cz[1] * t2 + cz[0] * t3 + cz[3];

        ribbonMesh.pts[i].x = x;
        ribbonMesh.pts[i].z = z;
        ribbonMesh.pts[i].y = 0.25f;
        ribbonMesh.pts[iPrec + i].x = x;
        ribbonMesh.pts[iPrec + i].z = z;
        ribbonMesh.pts[iPrec + i].y = -0.25f;
    }

    // The faces
    ribbonMesh.numFaces = 0;
    for( i = 0; i < numPts; i++ )
        ribbonMesh.norms[i] = ptZero;

    for( i = 0; i < iPrec - 1; i++ )
    {
        calcNorm( &norm, &ribbonMesh.pts[i + 1], &ribbonMesh.pts[i],
                         &ribbonMesh.pts[iPrec + i] );

        ribbonMesh.faces[i].material = 0;
        ribbonMesh.faces[i].norm = norm;

        ribbonMesh.norms[i] += norm;
        ribbonMesh.norms[iPrec + i] += norm;
        if( i != iPrec - 1 )
        {
            ribbonMesh.norms[i + 1] += norm;
            ribbonMesh.norms[iPrec + i + 1] += norm;
        }

        ribbonMesh.faces[i].p[0] = i;
        ribbonMesh.faces[i].p[1] = iPrec + i;
        ribbonMesh.faces[i].p[2] = i + 1;
        ribbonMesh.faces[i].p[3] = iPrec + i + 1;
        ribbonMesh.numFaces++;
    }

    ribbonMesh.numPoints = iPrec * 2;
    normalizeNorms( ribbonMesh.norms, ribbonMesh.numPoints );
}




//-----------------------------------------------------------------------------
// Name: initRibbonScene()
// Desc:
//-----------------------------------------------------------------------------
BOOL initRibbonScene()
{
    D3DXMATRIX matView, matTrans, matRotX, matRotY, matRotZ;

    iPrec = (int) (fTesselFact * 40.5);
    if( iPrec < 4 )
        iPrec = 4;

    if( !newMesh( &ribbonMesh, iPrec, iPrec * 2 ) )
        return FALSE;

    SetProjectionParams( TRUE, 2.2f, 2.2f, 0.0f, 3.0f );

    D3DXMatrixTranslation( &matTrans, 0.0f, 0.0f, 1.5f );
    D3DXMatrixRotationX( &matRotX, D3DXToRadian( 50.0f ) );
    D3DXMatrixRotationY( &matRotY, D3DXToRadian( 50.0f ) );
    D3DXMatrixRotationZ( &matRotZ, D3DXToRadian( 12.0f ) );
    matView = matRotZ * matRotY * matRotX * matTrans;
    g_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: delRibbonScene()
// Desc:
//-----------------------------------------------------------------------------
void delRibbonScene()
{
    delMesh( &ribbonMesh );
}




//-----------------------------------------------------------------------------
// Name: updateRibbonScene()
// Desc:
//-----------------------------------------------------------------------------
void updateRibbonScene( int flags, FLOAT fElapsedTime )
{
    D3DXMATRIX matWorld, matTrans, matRotX, matRotY, matRotZ;
    D3DXCOLOR color;
    FLOAT* pColor;
    FLOAT t;

    if( fElapsedTime > 0.25f )
        fElapsedTime = 0.25f;
    t = fElapsedTime * 20.0f;

    // Spin about the next axis when the window bounces
    if( gbBounced )
    {
        if( !bBounced )
        {
            if( xrotInc != 0.0 )
            {
                xrotInc = 0.0;
                yrotInc = 0.1;
            }
            else if( yrotInc != 0.0 )
            {
                yrotInc = 0.0;
                zrotInc = 0.1;
            }
            else if( zrotInc != 0.0 )
            {
                zrotInc = 0.0;
                xrotInc = 0.1;
            }
        }
        gbBounced = FALSE;
        bBounced = TRUE;
    }
    else
    {
        bBounced = FALSE;
    }

    D3DXMatrixRotationX( &matRotX, D3DXToRadian( xrot * (180.0 / PI) ) );
    D3DXMatrixRotationY( &matRotY, D3DXToRadian( yrot * (180.0 / PI) ) );
    D3DXMatrixRotationZ( &matRotZ, D3DXToRadian( zrot * (180.0 / PI) ) );
    matWorld = matRotZ * matRotY * matRotX;
    g_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

    genRibbon( t );

    if( !bColorCycle )
    {
        myglMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, (FLOAT*) &Material[1] );
    }
    else
    {
        HsvToRgb( hue, 1.0f, 1.0f, &color );
        myglMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, (FLOAT*) &color );

        hue += t;
        if( hue >= 360.0f )
            hue -= 360.0f;
    }

    updateObject( &ribbonMesh, bSmoothShading );
    updateObjectBack( &ribbonMesh, bSmoothShading );

    // The second ribbon: beside the first one, spinning the other way
    // round, in the complementary color
    if( flags & MF_TWO_RIBBONS )
    {
        D3DXMatrixTranslation( &matTrans, 0.05f, 0.0f, 0.0f );
        D3DXMatrixRotationX( &matRotX, D3DXToRadian( yrot * (180.0 / PI) ) );
        D3DXMatrixRotationY( &matRotY, D3DXToRadian( xrot * (180.0 / PI) ) );
        D3DXMatrixRotationZ( &matRotZ, D3DXToRadian( zrot * (180.0 / PI) ) );
        matWorld = matRotZ * matRotY * matRotX * matTrans;
        g_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

        if( !bColorCycle )
        {
            pColor = (FLOAT*) &Material[2];
        }
        else
        {
            color.r = 1.0f - color.r;
            color.g = 1.0f - color.g;
            color.b = 1.0f - color.b;
            pColor = (FLOAT*) &color;
        }
        myglMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, pColor );

        updateObject( &ribbonMesh, bSmoothShading );
        updateObjectBack( &ribbonMesh, bSmoothShading );
    }

    xrot += t * xrotInc;
    yrot += t * yrotInc;
    zrot += t * zrotInc;
}
