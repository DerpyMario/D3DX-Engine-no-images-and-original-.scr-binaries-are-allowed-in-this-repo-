//-----------------------------------------------------------------------------
// File: flag.cpp
//
// Desc: The "Textured Flag" style: a flag waving on its pole, with the
//       picture of the settings (or the default one) on it.
//
//       The flag is a strip of 15 quads; iPrec meshes hold the phases of
//       the wave, and are drawn one after the other.
//
//       (the flag module of the OpenGL version, ported to Direct3D 8)
//-----------------------------------------------------------------------------
#include <windows.h>
#include <math.h>
#include "ss3dfo.h"

#define MAX_FRAMES      20
#define FLAG_STRIPS     15      // quads across the flag
#define FLAG_MESH_SIZE  225


static FLOAT    phase;          // of the wave of the mesh being built
static FLOAT    xMin;           // the pole
static int      iFace;          // (not used)
static FLOAT    light1Ambient[4];
static FLOAT    light1Specular[4];
static TEXTURE  texture;        // (the OpenGL texture; not loaded by this port)
static FLOAT    fFrame;
static FLOAT    zrotInc;
static FLOAT    xrotInc;
static int      iFrame;         // the mesh to draw

static MESH     flagMesh[MAX_FRAMES];

static FLOAT    flagWidth = 0.75f;
static FLOAT    flagHeight = 0.5625f;
static int      iPrec = 10;     // number of meshes
static FLOAT    flagSpecular[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static FLOAT    light0Pos[4] = { 20.0f, 5.0f, 20.0f, 0.0f };
static FLOAT    light1Pos[4] = { -20.0f, 5.0f, 0.0f, 0.0f };
static FLOAT    light1Diffuse[4] = { 0.4f, 0.4f, 0.4f, 1.0f };
static FLOAT    flagColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static double   yrotInc = 3.0;
static double   zrot = 5.7;
static double   yrot = 23.0;
static double   xrot = 23.0;




//-----------------------------------------------------------------------------
// Name: findElemExact()
// Desc: The index of a point of the mesh, adding it if it is not there yet
//       (or if bSearch is FALSE).  The normal is added to the normal of the
//       point.
//-----------------------------------------------------------------------------
int findElemExact( MESH* mesh, int iStart, D3DXVECTOR3* pt, BOOL bSearch,
                   D3DXVECTOR3* norm )
{
    int i;
    D3DXVECTOR3* pts = mesh->pts + iStart;

    if( bSearch )
    {
        for( i = iStart; i < mesh->numPoints; i++, pts++ )
        {
            if( (pts->x == pt->x) && (pts->y == pt->y) && (pts->z == pt->z) )
            {
                mesh->norms[i].x += norm->x;
                mesh->norms[i].y += norm->y;
                mesh->norms[i].z += norm->z;
                return i;
            }
        }
    }
    else
    {
        i = mesh->numPoints;
    }

    mesh->pts[i] = *pt;
    mesh->norms[i] = *norm;
    mesh->numPoints++;

    return i;
}




//-----------------------------------------------------------------------------
// Name: getZ()
// Desc: The wave: grows from the pole to the end of the flag
//-----------------------------------------------------------------------------
static FLOAT getZ( FLOAT x )
{
    return (FLOAT) (sin( (1.0 / flagWidth) * (x - xMin) * (2 * PI) + phase ) *
                    sqrt( (flagWidth - (x - xMin)) * (1.0 / flagWidth) ) * 0.25);
}




//-----------------------------------------------------------------------------
// Name: genFlag()
// Desc: The flag at the current phase of the wave
//-----------------------------------------------------------------------------
static BOOL genFlag( MESH* mesh )
{
    int i;
    FLOAT x, xStep;
    D3DXVECTOR3 a, b, c, d;
    FACE* face;

    if( !newMesh( mesh, FLAG_MESH_SIZE, FLAG_MESH_SIZE ) )
        return FALSE;

    xStep = flagWidth * (1.0f / 16.0f);
    x = xMin;

    for( i = 0; i < FLAG_STRIPS; i++ )
    {
        face = &mesh->faces[mesh->numFaces];

        a.x = x;
        a.y = 0.0f;
        a.z = getZ( x );

        b.x = x;
        b.y = flagHeight;
        b.z = getZ( x );

        c.x = x + xStep;
        c.y = 0.0f;
        c.z = getZ( c.x );

        d.x = c.x;
        d.y = flagHeight;
        d.z = getZ( c.x );

        calcNorm( &face->norm, &c, &b, &a );
        face->material = 0;

        face->p[0] = findElemExact( mesh, 0, &a, TRUE, &face->norm );
        face->p[1] = findElemExact( mesh, 0, &b, TRUE, &face->norm );
        face->p[2] = findElemExact( mesh, 0, &c, TRUE, &face->norm );
        face->p[3] = findElemExact( mesh, 0, &d, TRUE, &face->norm );
        mesh->numFaces++;

        x = c.x;
    }

    normalizeNorms( mesh->norms, mesh->numPoints );

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: initFlagScene()
// Desc:
//-----------------------------------------------------------------------------
BOOL initFlagScene()
{
    D3DXMATRIX matView;
    D3DLIGHT8 light;
    FLOAT phaseStep;
    int i;

    SetProjectionParams( TRUE, 2.0f, 2.0f, 0.0f, 3.0f );

    D3DXMatrixTranslation( &matView, 0.0f, 0.0f, 1.5f );
    g_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    // Two lights
    g_pd3dDevice->GetLight( 0, &light );
    light.Position = D3DXVECTOR3( light0Pos[0], light0Pos[1], light0Pos[2] );
    g_pd3dDevice->SetLight( 0, &light );

    light.Ambient = *(D3DCOLORVALUE*) light1Ambient;
    light.Diffuse = *(D3DCOLORVALUE*) light1Diffuse;
    light.Specular = *(D3DCOLORVALUE*) light1Specular;
    light.Type = D3DLIGHT_POINT;
    light.Position = D3DXVECTOR3( light1Pos[0], light1Pos[1], light1Pos[2] );
    g_pd3dDevice->SetLight( 1, &light );
    g_pd3dDevice->LightEnable( 1, TRUE );

    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // The texture
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );

    // The phases of the wave
    iPrec = (int) (fTesselFact * 10.0f);
    if( iPrec < 5 )
        iPrec = 5;
    if( iPrec > MAX_FRAMES )
        iPrec = MAX_FRAMES;

    phaseStep = (2 * PI) / (FLOAT) iPrec;
    phase = 0.0f;
    for( i = 0; i < iPrec; i++ )
    {
        if( !genFlag( &flagMesh[i] ) )
            return FALSE;
        phase += phaseStep;
    }

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: delFlagScene()
// Desc:
//-----------------------------------------------------------------------------
void delFlagScene()
{
    int i;

    for( i = 0; i < iPrec; i++ )
        delMesh( &flagMesh[i] );

    ss_DeleteTexture( &texture );
}




//-----------------------------------------------------------------------------
// Name: updateFlagScene()
// Desc:
//-----------------------------------------------------------------------------
void updateFlagScene( int flags, FLOAT fElapsedTime )
{
    D3DXMATRIX matWorld, matRotX, matRotY, matRotZ;
    TEXVERTEX v[4];
    WORD indices[4] = { 0, 1, 2, 3 };
    MESH* mesh;
    FACE* face;
    FLOAT t, s, sNext;
    int i;

    if( fElapsedTime > 0.25f )
        fElapsedTime = 0.25f;
    t = fElapsedTime * 20.0f;

    s = 0.0f;

    D3DXMatrixRotationX( &matRotX, D3DXToRadian( (FLOAT) xrot ) );
    D3DXMatrixRotationY( &matRotY, D3DXToRadian( (FLOAT) yrot ) );
    D3DXMatrixRotationZ( &matRotZ, D3DXToRadian( (FLOAT) zrot ) );
    matWorld = matRotZ * matRotY * matRotX;
    g_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

    iFace = 0;

    myglMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, flagColor );
    myglMaterialfv( GL_FRONT_AND_BACK, GL_SPECULAR, flagSpecular );
    myglMaterialf( GL_FRONT_AND_BACK, GL_SHININESS, 60.0f );

    mesh = &flagMesh[iFrame];

    g_pd3dDevice->SetVertexShader( D3DFVF_TEXVERTEX );

    for( i = 0, face = mesh->faces; i < mesh->numFaces; i++, face++ )
    {
        v[0].p = mesh->pts[face->p[0]];
        v[1].p = mesh->pts[face->p[1]];
        v[2].p = mesh->pts[face->p[2]];
        v[3].p = mesh->pts[face->p[3]];

        sNext = s + (1.0f / FLAG_STRIPS);
        v[0].tu = s;
        v[0].tv = 1.0f;
        v[1].tu = s;
        v[1].tv = 0.0f;
        v[2].tu = sNext;
        v[2].tv = 1.0f;
        v[3].tu = sNext;
        v[3].tv = 0.0f;

        if( !bSmoothShading )
        {
            v[0].n = face->norm;
            v[1].n = face->norm;
            v[2].n = face->norm;
            v[3].n = face->norm;
        }
        else
        {
            v[0].n = mesh->norms[face->p[0]];
            v[1].n = mesh->norms[face->p[1]];
            v[2].n = mesh->norms[face->p[2]];
            v[3].n = mesh->norms[face->p[3]];
        }

        s = sNext;

        g_pd3dDevice->DrawIndexedPrimitiveUP( D3DPT_TRIANGLESTRIP, 0, 4, 2, indices,
                                              D3DFMT_INDEX16, v, sizeof(TEXVERTEX) );
    }

    // Swing to and fro
    xrot += xrotInc * t;
    yrot += yrotInc * t;
    zrot += zrotInc * t;
    if( ((yrot < -65.0) && (yrotInc < 0.0)) ||
        ((yrot > 25.0) && (yrotInc > 0.0)) )
    {
        yrotInc = -yrotInc;
    }

    // The next phase of the wave
    fFrame += t;
    iFrame = (int) fFrame;
    if( iFrame >= iPrec )
    {
        iFrame = 0;
        fFrame = 0.0f;
    }
}
