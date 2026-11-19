//-----------------------------------------------------------------------------
// File: winlogo.cpp
//
// Desc: The "Windows Logo" style: the waving Windows flag (the DDS picture),
//       semi-transparent.  Every 10 seconds the window flies back to where it
//       started, the flag stops, and the Windows name (the PNG picture) fades
//       in over it and out again.
//
//       (The OpenGL version drew the flag out of colored quads.)
//-----------------------------------------------------------------------------
#include <windows.h>
#include <math.h>
#include "ss3dfo.h"

#define MAX_FRAMES      20
#define LOGO_STRIPS     35      // quads across the flag
#define LOGO_MESH_SIZE  1225

// The states of the name
enum
{
    LOGO_FLYING = 0,            // the flag flies around
    LOGO_RETURNING,             // the window goes back to its origin
    LOGO_FADE_IN,               // the name fades in (1 s)
    LOGO_SHOWING,               // (5 s)
    LOGO_FADE_OUT,              // (1 s)
};

// The vertices of the name (transformed)
#define D3DFVF_TLVERTEX         (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)
struct TLVERTEX
{
    FLOAT    x, y, z, rhw;
    D3DCOLOR color;
    FLOAT    tu, tv;
};


static MESH     logoMesh[MAX_FRAMES];
static FLOAT    phase;          // of the wave of the mesh being built
static FLOAT    xMin;           // the pole
static double   yrot;
static int      iFrame;         // the mesh to draw
static FLOAT    fStateStart;
static int      iState;
static FLOAT    fTime;

static int      iPrec = 10;     // number of meshes
static FLOAT    logoSpecular[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static FLOAT    lightPos[3] = { -15.0f, 0.0f, -10.0f };
static double   zrot = -12.0;
static double   xrot = 40.0;
static FLOAT    fNextTime = 10.0f;




//-----------------------------------------------------------------------------
// Name: getZ()
// Desc: The wave: grows from the pole to the end of the flag
//-----------------------------------------------------------------------------
static FLOAT getZ( FLOAT x )
{
    return (FLOAT) -(sqrt( (0.75 - (x - xMin)) * (1.0 / 0.75) ) *
                     sin( (1.0 / 0.75) * (x - xMin) * (2 * PI) + phase ) * 0.125);
}




//-----------------------------------------------------------------------------
// Name: genLogo()
// Desc: The flag at the current phase of the wave
//-----------------------------------------------------------------------------
static BOOL genLogo( MESH* mesh )
{
    int i;
    FLOAT x;
    D3DXVECTOR3 a, b, c, d;
    FACE* face;

    if( !newMesh( mesh, LOGO_MESH_SIZE, LOGO_MESH_SIZE ) )
        return FALSE;

    x = xMin;
    for( i = 0; i < LOGO_STRIPS; i++ )
    {
        face = &mesh->faces[mesh->numFaces];

        a.x = x;
        a.y = 0.0f;
        a.z = getZ( x );

        b.x = x;
        b.y = 0.75f;
        b.z = getZ( x );

        c.x = x + (1.0f / 48.0f);
        c.y = 0.0f;
        c.z = getZ( c.x );

        d.x = c.x;
        d.y = 0.75f;
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
// Name: delWinLogoScene()
// Desc:
//-----------------------------------------------------------------------------
void delWinLogoScene()
{
    int i;

    for( i = 0; i < iPrec; i++ )
        delMesh( &logoMesh[i] );
}




//-----------------------------------------------------------------------------
// Name: initWinLogoScene()
// Desc:
//-----------------------------------------------------------------------------
BOOL initWinLogoScene()
{
    D3DXMATRIX matView;
    D3DLIGHT8 light;
    int i;

    SetProjectionParams( TRUE, 2.0f, 2.0f, 0.0f, 3.0f );

    D3DXMatrixTranslation( &matView, -0.17f, -0.04f, 1.5f );
    g_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    g_pd3dDevice->GetLight( 0, &light );
    light.Position = D3DXVECTOR3( lightPos[0], lightPos[1], lightPos[2] );
    g_pd3dDevice->SetLight( 0, &light );

    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // The flag is semi-transparent: the alpha of the texture times the
    // alpha of the material
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );

    // The phases of the wave
    phase = 0.0f;
    iPrec = MAX_FRAMES;
    for( i = 0; i < iPrec; i++ )
    {
        if( !genLogo( &logoMesh[i] ) )
            return FALSE;
        phase += PI / 10.0f;
    }

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: updateWinLogoScene()
// Desc:
//-----------------------------------------------------------------------------
void updateWinLogoScene( int flags, FLOAT fElapsedTime )
{
    static FLOAT fFrame = (FLOAT) iPrec;
    D3DXMATRIX matWorld, matRotX, matRotY, matRotZ, matScale;
    FLOAT color[4];
    FLOAT fAlpha;
    FLOAT t;
    FLOAT s = 0.0f;

    if( fElapsedTime > 0.25f )
        fElapsedTime = 0.25f;
    t = fElapsedTime * 20.0f;

    // The name
    fTime += fElapsedTime;
    if( fNextTime != -1.0f && fTime > fNextTime )
    {
        fStateStart = fTime;
        switch( iState )
        {
            case LOGO_FLYING:
                iState = LOGO_RETURNING;
                gbReturnToOrigin = TRUE;
                fNextTime = -1.0f;      // until the window is back
                break;
            case LOGO_RETURNING:
                iState = LOGO_FADE_IN;
                fNextTime = fTime + 1.0f;
                break;
            case LOGO_FADE_IN:
                iState = LOGO_SHOWING;
                fNextTime = fTime + 5.0f;
                break;
            case LOGO_SHOWING:
                iState = LOGO_FADE_OUT;
                fNextTime = fTime + 1.0f;
                break;
            case LOGO_FADE_OUT:
                iState = LOGO_FLYING;
                gbReturnToOrigin = FALSE;
                fNextTime = fTime + 10.0f;
                break;
        }
    }

    fAlpha = 0.0f;
    switch( iState )
    {
        case LOGO_RETURNING:
            // Back, and the flag is still: fade the name in
            if( gbAtOrigin && iFrame == 0 )
                fNextTime = fTime;
            break;
        case LOGO_FADE_IN:
            fAlpha = fTime - fStateStart;
            break;
        case LOGO_SHOWING:
            fAlpha = 1.0f;
            break;
        case LOGO_FADE_OUT:
            fAlpha = 1.0f - (fTime - fStateStart);
            break;
    }

    if( fAlpha != 0.0f )
    {
        FLOATRECT* pRect = g_pFloatRect;
        TLVERTEX v[4];
        FLOAT x0, y0, x1, y1;
        int iAlpha;
        D3DCOLOR dwColor;

        x0 = pRect->xMin - g_ptMonitorOrigin.x;
        x1 = x0 + pRect->xSize;
        y0 = pRect->yMin - g_ptMonitorOrigin.y;
        y1 = pRect->ySize + pRect->yMin;

        if( fAlpha >= 1.0f )
            iAlpha = 255;
        else if( fAlpha > 0.0f )
            iAlpha = (int) (fAlpha * 255.0f);
        else
            iAlpha = 0;
        dwColor = (iAlpha << 24) | 0x00ffffff;

        v[0].x = x0;  v[0].y = y0;  v[0].z = 0.9f;  v[0].rhw = 0.1f;
        v[0].color = dwColor;  v[0].tu = 0.0f;  v[0].tv = 0.0f;
        v[1].x = x1;  v[1].y = y0;  v[1].z = 0.9f;  v[1].rhw = 0.1f;
        v[1].color = dwColor;  v[1].tu = 1.0f;  v[1].tv = 0.0f;
        v[2].x = x0;  v[2].y = y1;  v[2].z = 0.9f;  v[2].rhw = 0.1f;
        v[2].color = dwColor;  v[2].tu = 0.0f;  v[2].tv = 1.0f;
        v[3].x = x1;  v[3].y = y1;  v[3].z = 0.9f;  v[3].rhw = 0.1f;
        v[3].color = dwColor;  v[3].tu = 1.0f;  v[3].tv = 1.0f;

        g_pd3dDevice->SetTexture( 0, g_pDeviceObjects->m_pTexture2 );
        g_pd3dDevice->SetVertexShader( D3DFVF_TLVERTEX );
        g_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof(TLVERTEX) );
    }

    // The flag
    g_pd3dDevice->SetTexture( 0, g_pDeviceObjects->m_pTexture );

    D3DXMatrixRotationX( &matRotX, D3DXToRadian( (FLOAT) xrot ) );
    D3DXMatrixRotationY( &matRotY, D3DXToRadian( (FLOAT) yrot ) );
    D3DXMatrixRotationZ( &matRotZ, D3DXToRadian( (FLOAT) zrot ) );
    D3DXMatrixScaling( &matScale, 0.82f, 0.92f, 0.82f );
    matWorld = matScale * matRotZ * matRotY * matRotX;
    g_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

    myglMaterialfv( GL_FRONT_AND_BACK, GL_SPECULAR, logoSpecular );
    myglMaterialf( GL_FRONT_AND_BACK, GL_SHININESS, 40.0f );

    // (fades out as the name fades in)
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = 1.0f - fAlpha;
    if( color[3] != 0.0f )
    {
        MESH* mesh = &logoMesh[iFrame];
        FACE* face;
        TEXVERTEX* pVertices;
        WORD* pIndices;
        int numVerts = 0;
        int numPrims = 0;
        int i;

        myglMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color );

        g_pVBTex->Lock( 0, 0, (BYTE**) &pVertices, 0 );
        g_pIB->Lock( 0, 5000, (BYTE**) &pIndices, 0 );

        for( i = 0, face = mesh->faces; i < mesh->numFaces; i++, face++ )
        {
            pVertices[0].p = mesh->pts[face->p[0]];
            pVertices[0].n = bSmoothShading ? -mesh->norms[face->p[0]] : -face->norm;
            pVertices[0].tu = s;
            pVertices[0].tv = 1.0f;

            pVertices[1].p = mesh->pts[face->p[1]];
            pVertices[1].n = bSmoothShading ? -mesh->norms[face->p[1]] : -face->norm;
            pVertices[1].tu = s;
            pVertices[1].tv = 0.0f;

            s += 1.0f / LOGO_STRIPS;

            pVertices[2].p = mesh->pts[face->p[2]];
            pVertices[2].n = bSmoothShading ? -mesh->norms[face->p[2]] : -face->norm;
            pVertices[2].tu = s;
            pVertices[2].tv = 1.0f;

            pVertices[3].p = mesh->pts[face->p[3]];
            pVertices[3].n = bSmoothShading ? -mesh->norms[face->p[3]] : -face->norm;
            pVertices[3].tu = s;
            pVertices[3].tv = 0.0f;

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

        g_pVBTex->Unlock();
        g_pIB->Unlock();

        g_pd3dDevice->SetVertexShader( D3DFVF_TEXVERTEX );
        g_pd3dDevice->SetStreamSource( 0, g_pVBTex, sizeof(TEXVERTEX) );
        g_pd3dDevice->SetIndices( g_pIB, 0 );
        g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, numVerts, 0, numPrims );
    }

    // The next phase of the wave (the flag stops while the name shows)
    if( iFrame != 0 ||
        (iState != LOGO_FADE_IN && iState != LOGO_SHOWING && iState != LOGO_FADE_OUT) )
    {
        fFrame -= t;
        iFrame = (int) fFrame;
        if( iFrame < 0 )
        {
            iFrame = iPrec - 1;
            fFrame = (FLOAT) iFrame;
        }
    }
}
