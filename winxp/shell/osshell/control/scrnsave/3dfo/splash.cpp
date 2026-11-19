//-----------------------------------------------------------------------------
// File: splash.cpp
//
// Desc: The "Splash" style: drops fall into water and make ripples.
//
//       The water is a disc of iPrec rings of iPrec points.  The height of
//       the rings is pushed outwards one ring per step, the drop morphs
//       from a sphere into a splash as it falls.
//
//       (splash.c of the OpenGL version, ported to Direct3D 8)
//-----------------------------------------------------------------------------
#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include "ss3dfo.h"

#define NUM_DROPS       10      // morph steps of the drop


static MESH         waterInmesh;    // heights of the rings
static MESH         waterMesh;      // reflected heights
static MESH         waterBorderMesh;// the side of the disc
static D3DXVECTOR3* splashCurve;    // profile of a splash
static MESH         waterOutmesh;   // the surface of the water
static MESH*        drops;          // the drop, from sphere to splash
static int          iPrec;          // points per ring, and rings
static D3DXVECTOR3* curves;         // the NUM_DROPS profiles of the drop
static D3DXVECTOR3* sphereCurve;    // profile of a sphere
static FLOAT        hue;            // color cycling
static double       rotY;
static FLOAT        waterTime;      // time since the water was regenerated
static double       ripples;
static double       dist;           // drop: distance to the water surface
static int          dropIdx;        // the drop to draw
static double       mag;            // magnitude of the waves
static double       xrot;
static double       zrot;

static FLOAT        fTesselMax = 0.35f;
static BOOL         bFirstTime = TRUE;
static double       rotYInc  = 0.1;
static double       theta    = 1.0;
static double       damp     = 1.0;
static double       freq     = 1.0;
static double       dropY    = 1.0;
static double       dropSize = 0.3;
static double       xrotInc  = 1.5;
static double       zrotInc  = 3.0;




//-----------------------------------------------------------------------------
// Name: genWater()
// Desc: Regenerates the surface of the water
//-----------------------------------------------------------------------------
static void genWater( double freq, double damp, double mag, double theta, double ripples )
{
    int i;
    int j;
    int jj;
    int facecount;
    double r;
    double angle;
    double dr = 1.0 / iPrec;
    double angleInc = 2.0 * PI * dr;
    D3DXVECTOR3 norm;

    // The grid of the surface (once)
    if( bFirstTime )
    {
        for( i = 0; i < iPrec; i++ )
        {
            angle = 0.0;
            for( j = 0; j < iPrec; j++ )
            {
                double c = cos( angle );
                double s = sin( angle );
                double len = 1.0 / sqrt( c * c + s * s );

                waterOutmesh.pts[i * iPrec + j].x = (FLOAT) (len * c * ((double) i / (double) (iPrec - 1)));
                waterOutmesh.pts[i * iPrec + j].z = (FLOAT) (len * s * ((double) i / (double) (iPrec - 1)));
                waterMesh.pts[i * iPrec + j].y = 0.0f;
                waterInmesh.pts[i * iPrec + j].y = 0.0f;

                angle += angleInc;
            }
        }
    }

    // Push the waves outwards; the drop starts new ones in the middle
    for( i = iPrec - 1, r = 1.0; i >= 0; i--, r -= dr )
    {
        FLOAT h;

        if( i == 0 )
        {
            if( ripples != 0.0 )
                h = (FLOAT) -(cos( r * freq + theta ) * exp( -r * damp * 0.5 ) * mag);
            else
                h = waterMesh.pts[0].y * 0.95f;
        }
        else
        {
            h = waterInmesh.pts[(i - 1) * iPrec].y * 0.95f;
        }

        for( j = 0; j < iPrec; j++ )
            waterInmesh.pts[i * iPrec + j].y = h;
    }

    // The reflected waves come back from the border
    for( i = 0; i < iPrec; i++ )
    {
        for( j = 0; j < iPrec; j++ )
        {
            if( i == iPrec - 1 )
                waterMesh.pts[i * iPrec + j].y = -waterInmesh.pts[i * iPrec + j].y;
            else
                waterMesh.pts[i * iPrec + j].y = waterMesh.pts[(i + 1) * iPrec + j].y * 0.95f;
        }
    }

    waterOutmesh.numFaces = 0;
    waterBorderMesh.numFaces = 0;

    for( i = 0; i < iPrec; i++ )
        for( j = 0; j < iPrec; j++ )
            waterOutmesh.norms[i * iPrec + j] = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );

    // The surface: both waves; the border stays flat
    for( i = 0; i < iPrec; i++ )
    {
        for( j = 0; j < iPrec; j++ )
        {
            D3DXVECTOR3* pt = &waterOutmesh.pts[i * iPrec + j];

            pt->y = waterInmesh.pts[i * iPrec + j].y + waterMesh.pts[i * iPrec + j].y;

            if( i == iPrec - 1 )
            {
                pt->y = 0.0f;

                waterBorderMesh.pts[j].x = pt->x;
                waterBorderMesh.pts[j].z = pt->z;
                waterBorderMesh.pts[j].y = pt->y;

                waterBorderMesh.pts[iPrec + j].x = pt->x;
                waterBorderMesh.pts[iPrec + j].z = pt->z;
                waterBorderMesh.pts[iPrec + j].y = -0.5f;
            }
        }
    }

    for( i = 0; i < 2; i++ )
        for( j = 0; j < iPrec; j++ )
            waterBorderMesh.norms[i * iPrec + j] = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );

    // The faces of the surface
    facecount = 0;
    for( i = 0; i < iPrec - 1; i++ )
    {
        for( j = 0; j < iPrec; j++ )
        {
            int p00, p10, p01, p11;

            jj = (j == iPrec - 1) ? 0 : j + 1;

            calcNorm( &norm, &waterOutmesh.pts[(i + 1) * iPrec + j],
                             &waterOutmesh.pts[i * iPrec + j],
                             &waterOutmesh.pts[i * iPrec + jj] );

            if( (norm.x > -0.000001f) && (norm.x < 0.000001f) &&
                (norm.y > -0.000001f) && (norm.y < 0.000001f) &&
                (norm.z > -0.000001f) && (norm.z < 0.000001f) )
            {
                calcNorm( &norm, &waterOutmesh.pts[i * iPrec + jj],
                                 &waterOutmesh.pts[(i + 1) * iPrec + jj],
                                 &waterOutmesh.pts[(i + 1) * iPrec + j] );
            }

            waterOutmesh.faces[facecount].material = 0;
            waterOutmesh.faces[facecount].norm = norm;

            p00 = i * iPrec + j;
            p10 = (i + 1) * iPrec + j;
            p01 = i * iPrec + jj;
            p11 = (i + 1) * iPrec + jj;

            waterOutmesh.norms[p00] += norm;
            waterOutmesh.norms[p10] += norm;
            waterOutmesh.norms[p01] += norm;
            waterOutmesh.norms[p11] += norm;

            waterOutmesh.faces[facecount].p[0] = p10;
            waterOutmesh.faces[facecount].p[1] = p00;
            waterOutmesh.faces[facecount].p[2] = p11;
            waterOutmesh.faces[facecount].p[3] = p01;
            waterOutmesh.numFaces++;
            facecount++;
        }
    }

    waterOutmesh.numPoints = iPrec * iPrec;

    // The faces of the border
    for( j = 0; j < iPrec; j++ )
    {
        jj = (j == iPrec - 1) ? 0 : j + 1;

        calcNorm( &norm, &waterBorderMesh.pts[iPrec + j],
                         &waterBorderMesh.pts[j],
                         &waterBorderMesh.pts[jj] );

        waterBorderMesh.faces[j].material = 0;
        waterBorderMesh.faces[j].norm = norm;

        waterBorderMesh.norms[j] = norm;
        if( j != 0 )
            waterBorderMesh.norms[j - 1] = norm;
        waterBorderMesh.norms[iPrec + j] = norm;
        waterBorderMesh.norms[jj] = norm;

        waterBorderMesh.faces[j].p[0] = iPrec + j;
        waterBorderMesh.faces[j].p[1] = j;
        waterBorderMesh.faces[j].p[2] = iPrec + jj;
        waterBorderMesh.faces[j].p[3] = jj;
        waterBorderMesh.numFaces++;
    }

    waterBorderMesh.numPoints = iPrec * 2;

    normalizeNorms( waterBorderMesh.norms, waterBorderMesh.numPoints );
    normalizeNorms( waterOutmesh.norms, waterOutmesh.numPoints );

    bFirstTime = FALSE;
}




//-----------------------------------------------------------------------------
// Name: genCurves()
// Desc: The profiles of the drop: NUM_DROPS steps from a sphere to a splash
//-----------------------------------------------------------------------------
static void genCurves()
{
    int i, j;
    double angle;
    double r, x, y;
    FLOAT fSplash, fSphere, fStep;
    D3DXVECTOR3* pCurve;

    if( iPrec < 2 )
        return;

    FLOAT fPrec = (FLOAT) (iPrec - 1);
    double sinA = sin( PI / 4 );
    double cosA = cos( PI / 4 );

    fSphere = 0.0f;
    fSplash = 1.0f;
    fStep = 1.0f / (FLOAT) (iPrec - 1);

    // half a circle
    angle = PI / 2;
    for( i = 0; i < iPrec; i++ )
    {
        sphereCurve[i].x = (FLOAT) (cos( angle ) * 0.5);
        sphereCurve[i].y = (FLOAT) (sin( angle ) * 0.5);
        angle += -PI / fPrec;
    }

    // a petal, turned by 45 degrees
    angle = PI / 4;
    for( i = 0; i < iPrec; i++ )
    {
        r = sin( angle + angle );
        if( r < 0.0 )
            r = -r;

        x = cos( angle ) * sqrt( r ) * 1.5;
        y = sin( angle ) * sqrt( r ) * 1.5;

        splashCurve[i].x = (FLOAT) (x * cosA - y * sinA);
        splashCurve[i].y = (FLOAT) ((x * sinA + y * cosA) - 1.0);
        angle += -(PI / 4) / fPrec;
    }

    // the steps in between
    pCurve = curves;
    for( j = 0; j < NUM_DROPS; j++ )
    {
        for( i = 0; i < iPrec; i++, pCurve++ )
        {
            pCurve->x = splashCurve[i].x * fSplash + sphereCurve[i].x * fSphere;
            pCurve->y = sphereCurve[i].y * fSphere + splashCurve[i].y * fSplash;
            pCurve->z = 0.0f;
        }

        fSphere += fStep;
        fSplash -= fStep;
    }
}




//-----------------------------------------------------------------------------
// Name: initSplashScene()
// Desc:
//-----------------------------------------------------------------------------
BOOL initSplashScene()
{
    D3DXMATRIX matView;
    int i;

    iPrec = (int) (fTesselFact * 10.5);
    if( iPrec < 4 )
        iPrec = 4;

    if( fTesselFact > fTesselMax )
        fTesselMax = fTesselFact;

    sphereCurve = (D3DXVECTOR3*) SaverAlloc( iPrec * sizeof(D3DXVECTOR3) );
    if( !sphereCurve )
        return FALSE;

    splashCurve = (D3DXVECTOR3*) SaverAlloc( iPrec * sizeof(D3DXVECTOR3) );
    if( !splashCurve )
        return FALSE;

    curves = (D3DXVECTOR3*) SaverAlloc( iPrec * NUM_DROPS * sizeof(D3DXVECTOR3) );
    if( !curves )
        return FALSE;

    drops = (MESH*) SaverAlloc( NUM_DROPS * sizeof(MESH) );
    if( !drops )
        return FALSE;

    SetProjectionParams( TRUE, 3.0f, 3.0f, 0.0f, 3.0f );

    D3DXVECTOR3 vEye( 0.0f, 0.0f, 1.5f );
    D3DXVECTOR3 vAt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtLH( &matView, &vEye, &vAt, &vUp );
    g_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    if( !newMesh( &waterMesh, iPrec * iPrec, (iPrec + 1) * iPrec ) )
        return FALSE;
    if( !newMesh( &waterInmesh, iPrec * iPrec, (iPrec + 1) * iPrec ) )
        return FALSE;
    if( !newMesh( &waterOutmesh, iPrec * iPrec, (iPrec + 1) * iPrec ) )
        return FALSE;
    if( !newMesh( &waterBorderMesh, iPrec, iPrec * 2 ) )
        return FALSE;

    genCurves();

    for( i = 0; i < NUM_DROPS; i++ )
        revolveSurface( &drops[i], &curves[iPrec * i], iPrec );

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: delSplashScene()
// Desc:
//-----------------------------------------------------------------------------
void delSplashScene()
{
    int i;

    for( i = 0; i < NUM_DROPS; i++ )
        delMesh( &drops[i] );

    SaverFree( sphereCurve );
    SaverFree( splashCurve );
    SaverFree( curves );
    SaverFree( drops );

    delMesh( &waterOutmesh );
    delMesh( &waterMesh );
    delMesh( &waterInmesh );
    delMesh( &waterBorderMesh );
}




//-----------------------------------------------------------------------------
// Name: updateSplashScene()
// Desc:
//-----------------------------------------------------------------------------
void updateSplashScene( int flags, FLOAT fElapsedTime )
{
    D3DXMATRIX matWorld, matRotZ, matRotX, matTrans, matRotY, matScale, matFlip;
    D3DXCOLOR color;
    FLOAT t;

    t = fElapsedTime * 20.0f;
    if( t > 0.25f )
        t = 0.25f;

    // Rock the water
    zrot += t * zrotInc;
    if( zrot >= 45.0 )
    {
        zrot = 45.0;
        zrotInc = -(FRAND() * 3.0 + 2.0);
    }
    else if( zrot <= -45.0 )
    {
        zrot = -45.0;
        zrotInc = FRAND() * 3.0 + 2.0;
    }

    xrot += t * xrotInc;
    if( xrot >= 10.0 )
    {
        xrot = 10.0;
        xrotInc = -(FRAND() * 2.0 + 1.0);
    }
    else if( zrot <= -10.0 )      // (sic)
    {
        xrot = -10.0;
        xrotInc = FRAND() * 2.0 + 1.0;
    }

    // A new drop once the last one is gone and the water has calmed down
    if( (dropY + 0.5 < -dropSize) && (mag < 0.05) )
    {
        dropIdx = 0;
        dropSize = rand() / (RAND_MAX * 6.0) + 0.1;
        dropY = 1.0;
    }

    dist = dropY + 0.5;
    if( (dist > -0.5 * dropSize) && (dist < 0.5 * dropSize) )
    {
        // The drop hits the water
        if( dist > 0.0 )
            dist = 0.5 * dropSize - dist;
        else
            dist = 0.5 * dropSize;

        freq = (PI / 4) / dist;
        if( freq < 0.2 )
            freq = 0.2;

        ripples = dropSize;
        damp = 20.0;
        mag = dist * 0.2 + 0.35 / fTesselMax;
        theta = 0.0;
    }
    else
    {
        // The waves die down
        ripples -= t * 0.05;
        if( ripples < 0.0 )
            ripples = 0.0;

        mag = t * mag * 0.95;
        if( ripples == 0.0 )
        {
            theta -= t * (PI / 6);
            mag = t * mag * 0.75;
        }

        if( damp > 0.0 )
            damp -= t;
    }

    waterTime += fElapsedTime;
    if( waterTime > 0.07f )
    {
        genWater( freq, damp, mag, theta, ripples );
        waterTime = 0.0f;
    }

    // The water
    D3DXMatrixRotationZ( &matRotZ, D3DXToRadian( (FLOAT) zrot ) );
    D3DXMatrixRotationX( &matRotX, D3DXToRadian( 30.0f ) );
    D3DXMatrixTranslation( &matTrans, 0.0f, -0.5f, 0.0f );
    D3DXMatrixRotationY( &matRotY, D3DXToRadian( rotY * (180.0 / PI) ) );
    matWorld = matRotY * matTrans * matRotX * matRotZ;
    g_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

    if( bColorCycle )
    {
        HsvToRgb( hue, 1.0f, 1.0f, &color );
        myglMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, (FLOAT*) &color );

        hue += t;
        if( hue >= 360.0f )
            hue -= 360.0f;
    }
    else
    {
        myglMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, (FLOAT*) &Material[6] );
    }

    updateObject( &waterOutmesh, bSmoothShading );

    if( !bColorCycle )
        myglMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, (FLOAT*) &Material[2] );

    updateObject( &waterBorderMesh, FALSE );

    // The drop
    if( dist > -dropSize )
    {
        if( !bColorCycle )
            myglMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, (FLOAT*) &Material[6] );

        D3DXMatrixRotationZ( &matRotZ, D3DXToRadian( (FLOAT) zrot ) );
        D3DXMatrixRotationX( &matRotX, D3DXToRadian( 30.0f ) );
        D3DXMatrixTranslation( &matTrans, 0.0f, (FLOAT) dropY, 0.0f );
        D3DXMatrixScaling( &matScale, (FLOAT) dropSize, (FLOAT) dropSize, (FLOAT) dropSize );
        D3DXMatrixRotationX( &matFlip, PI );
        matWorld = matFlip * matScale * matTrans * matRotX * matRotZ;
        g_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

        updateObject( &drops[dropIdx], bSmoothShading );
    }

    rotY += t * rotYInc;
    dropY -= t * 0.08;

    dropIdx = (int) (9.0 - dropY * 9.0);
    if( dropIdx > 9 )
        dropIdx = 9;
}
