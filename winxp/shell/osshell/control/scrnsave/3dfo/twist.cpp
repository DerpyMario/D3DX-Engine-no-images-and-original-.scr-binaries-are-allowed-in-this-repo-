//-----------------------------------------------------------------------------
// File: twist.cpp
//
// Desc: The "Twist" style: a tube along a closed figure-of-eight-like
//       curve, whose ends move up and down.  It changes the axis it spins
//       about whenever its window bounces.
//
//       (twist.c of the OpenGL version, ported to Direct3D 8)
//-----------------------------------------------------------------------------
#include <windows.h>
#include <math.h>
#include <string.h>
#include "ss3dfo.h"

#define XC_PTS          10      // points of the cross section (the last
                                // one closes it)
#define XC_FACES        (XC_PTS - 1)


static double*      ySpine;         // the curve, and a point a little further
static double*      ySpine2;
static double*      xSpine2;
static MESH         twistMesh;
static double*      xSpine;
static D3DXVECTOR3  crossSection[XC_PTS];
static FLOAT        twistAmp;       // height of the twist, -1 .. 1
static FLOAT        hue;
static double       zrot;
static double       zrotInc;
static double       yrot;
static double       xrot;
static double       xrotInc;

static double       tilt = 0.2;     // (not used)
static int          iPrec = 32;
static FLOAT        twistInc = 0.05f;
static double       tiltInc = 0.1;
static double       yrotInc = 0.1;




//-----------------------------------------------------------------------------
// Name: calcSpine()
// Desc: The point i (of n) of the curve, in polar coordinates
//-----------------------------------------------------------------------------
static void calcSpine( double* pAngle, double* pRadius, double i, double n )
{
    double angle;
    double r;

    angle = (i * PI) / (n - 1.0);
    if( angle >= PI )
        angle -= PI;
    if( angle > PI / 2 )
        angle = 2 * PI - angle;

    *pAngle = angle;

    r = sin( angle + angle );
    if( r < 0.0 )
        r = 0.0;

    *pRadius = sqrt( r ) * 0.5;
}




//-----------------------------------------------------------------------------
// Name: genSpine()
// Desc: The points of the curve, and points a little further on it (for the
//       direction of the curve)
//-----------------------------------------------------------------------------
static void genSpine( int n )
{
    int i;
    double angle, r;

    for( i = 0; i < n; i++ )
    {
        calcSpine( &angle, &r, (double) i, (double) iPrec );
        xSpine[i] = cos( angle ) * r;
        ySpine[i] = sin( angle ) * r;

        calcSpine( &angle, &r, (double) i + 0.00001, (double) iPrec );
        xSpine2[i] = cos( angle ) * r;
        ySpine2[i] = sin( angle ) * r;
    }
}




//-----------------------------------------------------------------------------
// Name: spineMatrix()
// Desc: Turns the cross section along the curve at point i, and moves it
//       there (z: the twist)
//-----------------------------------------------------------------------------
static void spineMatrix( MATRIX* mat, int i, double z )
{
    double x, y, dx, dy, len, angle;

    x = xSpine[i];
    y = ySpine[i];
    dx = xSpine2[i] - x;
    dy = ySpine2[i] - y;

    len = sqrt( dx * dx + dy * dy );
    if( len <= 0.0 )
        len = 0.0;
    else
        len = dy / len;
    if( dy < 0.0 )
        len = -len;

    angle = asin( len );
    if( dx < 0.0 )
        angle = PI - angle;
    if( dy < 0.0 )
        angle = -angle;
    if( angle < 0.0 )
        angle += 2 * PI;

    matrixIdent( mat );
    matrixRotate( mat, 0.0, 0.0, -angle );
    matrixTranslate( mat, x, y, z );
}




//-----------------------------------------------------------------------------
// Name: genTwist()
// Desc: The tube
//-----------------------------------------------------------------------------
static void genTwist()
{
    int i, j, jj, k;
    int iBase = 0;
    int facecount = 0;
    MATRIX mat;
    D3DXVECTOR3 ring0[XC_PTS];
    D3DXVECTOR3 ring1[XC_PTS];
    D3DXVECTOR3 norm;
    int p[4];

    twistMesh.numPoints = 0;
    twistMesh.numFaces = 0;

    for( i = 0; i < iPrec * XC_FACES - XC_FACES; i++ )
        twistMesh.norms[i] = ptZero;

    for( i = 0; i < iPrec - 1; i++ )
    {
        // The cross sections at the ends of the segment
        spineMatrix( &mat, i, cos( (i * 2 * PI) / (iPrec - 1.0) ) * twistAmp );
        for( k = 0; k < XC_PTS; k++ )
            xformPoint( &ring0[k], &crossSection[k], &mat );

        spineMatrix( &mat, i + 1, cos( ((i + 1.0) * 2 * PI) / (iPrec - 1.0) ) * twistAmp );
        for( k = 0; k < XC_PTS; k++ )
            xformPoint( &ring1[k], &crossSection[k], &mat );

        iBase += XC_FACES;

        memcpy( &twistMesh.pts[i * XC_FACES], ring0, XC_FACES * sizeof(D3DXVECTOR3) );
        twistMesh.numPoints += XC_FACES;

        // The faces between them (the last segment closes the tube)
        for( j = 0; j < XC_FACES; j++ )
        {
            jj = (j == XC_FACES - 1) ? 0 : j + 1;

            calcNorm( &norm, &ring1[j + 1], &ring1[j], &ring0[j] );

            twistMesh.faces[facecount].material = 3;
            twistMesh.faces[facecount].norm = norm;

            if( i == iPrec - 2 )
            {
                twistMesh.faces[facecount].p[0] = j;
                twistMesh.faces[facecount].p[1] = jj;
            }
            else
            {
                twistMesh.faces[facecount].p[0] = j + iBase;
                twistMesh.faces[facecount].p[1] = jj + iBase;
            }
            twistMesh.faces[facecount].p[2] = iBase - XC_FACES + j;
            twistMesh.faces[facecount].p[3] = iBase - XC_FACES + jj;

            p[0] = twistMesh.faces[facecount].p[0];
            p[1] = twistMesh.faces[facecount].p[1];
            p[2] = twistMesh.faces[facecount].p[2];
            p[3] = twistMesh.faces[facecount].p[3];
            for( k = 0; k < 4; k++ )
                twistMesh.norms[p[k]] += norm;

            twistMesh.numFaces++;
            facecount++;
        }
    }

    normalizeNorms( twistMesh.norms, twistMesh.numPoints );

    // The twist goes up and down
    if( twistAmp >= 1.0f )
        twistInc = -0.01f;
    else if( twistAmp <= -1.0f )
        twistInc = 0.01f;

    twistAmp += twistInc;
}




//-----------------------------------------------------------------------------
// Name: initTwistScene()
// Desc:
//-----------------------------------------------------------------------------
BOOL initTwistScene()
{
    D3DXMATRIX matView;
    int i;

    iPrec = (int) (fTesselFact * 32.5);
    if( iPrec < 5 )
        iPrec = 5;

    xSpine = (double*) SaverAlloc( iPrec * sizeof(double) );
    if( !xSpine )
        return FALSE;
    ySpine = (double*) SaverAlloc( iPrec * sizeof(double) );
    if( !ySpine )
        return FALSE;
    xSpine2 = (double*) SaverAlloc( iPrec * sizeof(double) );
    if( !xSpine2 )
        return FALSE;
    ySpine2 = (double*) SaverAlloc( iPrec * sizeof(double) );
    if( !ySpine2 )
        return FALSE;

    SetProjectionParams( TRUE, 3.0f, 3.0f, 0.0f, 3.0f );

    D3DXVECTOR3 vEye( 0.0f, 0.0f, 1.5f );
    D3DXVECTOR3 vAt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtLH( &matView, &vEye, &vAt, &vUp );
    g_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    if( !newMesh( &twistMesh, iPrec * XC_FACES - XC_FACES, iPrec * XC_FACES - XC_FACES ) )
        return FALSE;

    // The cross section: a circle in the yz plane
    for( i = 0; i < XC_PTS; i++ )
    {
        double angle = i * 2 * PI / 9.0;

        crossSection[i].x = 0.0f;
        crossSection[i].y = (FLOAT) (cos( angle ) * 0.15);
        crossSection[i].z = (FLOAT) (sin( angle ) * 0.15);
    }

    genSpine( iPrec );

    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );

    myglMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, (FLOAT*) &Material[3] );

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: delTwistScene()
// Desc:
//-----------------------------------------------------------------------------
void delTwistScene()
{
    delMesh( &twistMesh );

    SaverFree( xSpine );
    SaverFree( ySpine );
    SaverFree( xSpine2 );
    SaverFree( ySpine2 );
}




//-----------------------------------------------------------------------------
// Name: updateTwistScene()
// Desc:
//-----------------------------------------------------------------------------
void updateTwistScene( int flags, FLOAT fElapsedTime )
{
    D3DXMATRIX matView, matWorld, matTrans, matRotX, matRotY, matRotZ;
    D3DXCOLOR color;
    MATRIX model;
    FLOAT t;

    t = fElapsedTime * 20.0f;
    if( t > 0.25f )
        t = 0.25f;

    xrot += xrotInc * t;
    yrot += yrotInc * t;
    zrot += zrotInc * t;

    // Spin about the next axis when the window bounces
    if( gbBounced )
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
        gbBounced = FALSE;
    }

    tilt = t * (FLOAT) tiltInc + (FLOAT) tilt;
    if( (FLOAT) tilt >= PI / 4 )
    {
        tilt = PI / 4;
        tiltInc = -0.03;
    }
    else if( tilt <= -PI / 4 )
    {
        tilt = -PI / 4;
        tiltInc = 0.03;
    }

    genTwist();

    if( bColorCycle )
    {
        HsvToRgb( hue, 1.0f, 1.0f, &color );
        myglMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, (FLOAT*) &color );

        hue += t;
        if( hue >= 360.0f )
            hue -= 360.0f;
    }

    D3DXVECTOR3 vAt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );
    D3DXVECTOR3 vEye( 0.0f, 0.0f, 1.5f );
    D3DXMatrixLookAtLH( &matView, &vEye, &vAt, &vUp );
    g_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    D3DXMatrixTranslation( &matTrans, 0.0f, -0.5f, 0.0f );
    D3DXMatrixRotationX( &matRotX, D3DXToRadian( xrot * (180.0 / PI) ) );
    D3DXMatrixRotationY( &matRotY, D3DXToRadian( yrot * (180.0 / PI) ) );
    D3DXMatrixRotationZ( &matRotZ, D3DXToRadian( zrot * (180.0 / PI) ) );
    matWorld = matRotZ * matRotY * matRotX * matTrans;
    g_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

    // (the model matrix of the OpenGL version; not used)
    matrixIdent( &model );
    matrixRotate( &model, xrot, yrot, zrot );
    matrixTranslate( &model, 0.0, -0.5, 0.0 );

    updateObject( &twistMesh, bSmoothShading );
}
