//-----------------------------------------------------------------------------
// File: arc.cpp
//
// Desc: One "arc" of the Aurora screen saver.
//
//       Original object (4531 bytes of code):
//           shell\osshell\control\scrnsave\aurora\objfre\amd64\arc.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <math.h>
#include <d3d9.h>
#include "random.h"
#include "color.h"
#include "noise.h"
#include "matrix44.h"
#include "texture.h"
#include "arc.h"
#include "resource.h"




//-----------------------------------------------------------------------------
// Name: ArcClass::ArcClass()
//-----------------------------------------------------------------------------
ArcClass::ArcClass()
{
    for( int i = 0; i < AURORA_MAX_DEVICES; i++ )
    {
        m_pVB[i] = NULL;
        m_pTexture[i] = NULL;
    }

    m_fHeight        = 1.0f;
    m_nOctaves       = 5;
    m_bAbsoluteNoise = false;

    // The two texture layers drift slowly in opposite directions
    m_fTexScrollSpeed[0] =    RandomFloat() * 0.00002f + 0.00001f;
    m_fTexScrollSpeed[1] = -( RandomFloat() * 0.00002f + 0.00001f );
    m_fTexScroll[0] = RandomFloat() - RandomFloat();
    m_fTexScroll[1] = RandomFloat() - RandomFloat();

    // Random start position and drift direction in the noise field.  (The
    // shipped code draws the components in z, y, x order.)
    m_noisePos.z = ( RandomFloat() - RandomFloat() ) * 0.02f;
    m_noisePos.y = ( RandomFloat() - RandomFloat() ) * 0.02f;
    m_noisePos.x = ( RandomFloat() - RandomFloat() ) * 0.02f;

    m_noiseVelocity.z = RandomFloat() - RandomFloat();
    m_noiseVelocity.y = RandomFloat() - RandomFloat();
    m_noiseVelocity.x = RandomFloat() - RandomFloat();

    m_noiseBounds = Point3f( 20.0f, 20.0f, 20.0f );

    // NOTE: not in the original.  The shipped constructor never initialises
    // these two members, although Update() reads them when the noise position
    // reaches m_noiseBounds (which takes the better part of half an hour at the
    // default speed).  The AuroraScreensaver object lives on WinMain's stack,
    // deep enough that the members normally sit in untouched, zero-filled stack
    // pages - so zero is what the original effectively runs with.
    m_fMinNoiseSpeed = 0.0f;
    m_fMaxNoiseSpeed = 0.0f;
}




//-----------------------------------------------------------------------------
// Name: ArcClass::InitDevice()
// Desc: Creates the vertex buffer and the texture for one render unit.
//-----------------------------------------------------------------------------
void ArcClass::InitDevice( IDirect3DDevice9* pd3dDevice, int iDevice, float fBrightness )
{
    if( m_pVB[iDevice] == NULL )
    {
        ARCVERTEX* pVertices = NULL;

        pd3dDevice->CreateVertexBuffer( sizeof(m_Vertices), D3DUSAGE_WRITEONLY, D3DFVF_ARCVERTEX,
                                        D3DPOOL_MANAGED, &m_pVB[iDevice], NULL );
        m_pVB[iDevice]->Lock( 0, sizeof(m_Vertices), (void**)&pVertices, 0 );

        // Pick a random colour and make red its weakest component, so the arc
        // comes out somewhere between green and blue.
        float b = RandomFloat();
        float g = RandomFloat();
        float r = RandomFloat();
        if( g < r )
        {
            float t = r; r = g; g = t;
        }
        if( b < r )
        {
            float t = r; r = b; b = t;
        }
        DWORD dwColor = ColorToARGB( r * fBrightness, g * fBrightness, b * fBrightness, 1.0f );

        // A straight ribbon from (-10, 0, 0) to (10, 0, 0), m_fHeight tall
        const Point3f ptStart( -10.0f, 0.0f, 0.0f );
        const Point3f ptEnd( 10.0f, 0.0f, 0.0f );

        for( int i = 0; i <= ARC_SEGMENTS; i++ )
        {
            ARCVERTEX* pBottom = &pVertices[i * 2];
            ARCVERTEX* pTop    = &pVertices[i * 2 + 1];
            float t = (float)i / (float)ARC_SEGMENTS;

            pBottom->color = dwColor;
            pTop->color    = dwColor;

            pBottom->pos = ptStart * ( 1.0f - t ) + ptEnd * t;
            pTop->pos    = pBottom->pos + Point3f( 0.0f, 0.0f, m_fHeight );

            pBottom->tu0 = t;
            pBottom->tv0 = 0.0f;
            pTop->tu0    = t;
            pTop->tv0    = 1.0f;

            pBottom->tu1 = t;
            pBottom->tv1 = 0.0f;
            pTop->tu1    = t;
            pTop->tv1    = 1.0f;
        }

        memcpy( m_BaseVertices, pVertices, sizeof(m_BaseVertices) );
        memcpy( m_Vertices, pVertices, sizeof(m_Vertices) );

        m_pVB[iDevice]->Unlock();
    }

    if( m_pTexture[iDevice] == NULL )
    {
        Texture::LoadTextureFromResource( pd3dDevice, MAKEINTRESOURCEW( IDR_ARC_TEXTURE ), L"BMP",
                                          &m_pTexture[iDevice] );
    }
}




//-----------------------------------------------------------------------------
// Name: BounceNoiseVelocity()
// Desc: Reverses the drift along one axis once the position leaves the
//       allowed range.  (Expanded inline three times in the shipped image.)
//-----------------------------------------------------------------------------
static inline void BounceNoiseVelocity( float fPos, float fBound, float fMinSpeed, float fMaxSpeed,
                                        float* pfVelocity )
{
    if( *pfVelocity > 0.0f && fPos > fBound )
    {
        *pfVelocity = -( ( fMaxSpeed - fMinSpeed ) * RandomFloat() + fMinSpeed );
    }
    else if( *pfVelocity < 0.0f && fPos < -fBound )
    {
        *pfVelocity = ( fMaxSpeed - fMinSpeed ) * RandomFloat() + fMinSpeed;
    }
}




//-----------------------------------------------------------------------------
// Name: ArcClass::Update()
// Desc: Animates the ribbon.  fBrightness is not used (it only matters when
//       the vertex colours are generated in InitDevice()).
//-----------------------------------------------------------------------------
void ArcClass::Update( Camera camera, float fSpeed, float fBrightness, float fAmplitude )
{
    int i;

    (void)fBrightness;

    // Drift through the noise field
    m_noisePos += m_noiseVelocity * fSpeed;

    BounceNoiseVelocity( m_noisePos.x, m_noiseBounds.x, m_fMinNoiseSpeed, m_fMaxNoiseSpeed, &m_noiseVelocity.x );
    BounceNoiseVelocity( m_noisePos.y, m_noiseBounds.y, m_fMinNoiseSpeed, m_fMaxNoiseSpeed, &m_noiseVelocity.y );
    BounceNoiseVelocity( m_noisePos.z, m_noiseBounds.z, m_fMinNoiseSpeed, m_fMaxNoiseSpeed, &m_noiseVelocity.z );

    // Displace every pair of vertices by the turbulence at its rest position.
    // The bottom edge only moves sideways; the top edge also stretches upwards.
    for( i = 0; i <= ARC_SEGMENTS; i++ )
    {
        const ARCVERTEX* pBaseBottom = &m_BaseVertices[i * 2];
        const ARCVERTEX* pBaseTop    = &m_BaseVertices[i * 2 + 1];

        float fNoise = Turbulence( pBaseBottom->pos * 0.3f + m_noisePos, m_nOctaves, m_bAbsoluteNoise );

        m_Vertices[i * 2 + 1].pos = pBaseTop->pos +
                                    Point3f( fNoise * fAmplitude, fNoise * fAmplitude, fNoise * 2.0f + 2.0f );
        m_Vertices[i * 2].pos     = pBaseBottom->pos +
                                    Point3f( fNoise * fAmplitude, fNoise * fAmplitude, 0.0f );
    }

    // Scroll the two texture layers
    m_fTexScroll[0] += m_fTexScrollSpeed[0];
    if( m_fTexScroll[0] > 1.0f )
        m_fTexScroll[0] -= 1.0f;

    m_fTexScroll[1] += m_fTexScrollSpeed[1];
    if( m_fTexScroll[1] > 1.0f )
        m_fTexScroll[1] -= 1.0f;

    // Fade the ribbon where it runs across the line of sight and let it glow
    // where it runs along it: alpha = ( tangent . view direction ) ^ 4
    for( i = 0; i <= ARC_SEGMENTS; i++ )
    {
        const Point3f& pos = m_Vertices[i * 2].pos;

        // (sic) For the last pair the "next" vertex is one past the end of
        // m_Vertices, i.e. the first element of m_BaseVertices, and for the
        // first pair the "previous" vertex wraps around to the last pair.
        const Point3f& posNext = ( i < ARC_SEGMENTS ) ? m_Vertices[i * 2 + 2].pos
                                                     : m_BaseVertices[0].pos;
        const Point3f& posPrev = m_Vertices[( i * 2 + ARC_NUM_VERTICES - 2 ) % ARC_NUM_VERTICES].pos;

        Point3f toNext = ( pos - posNext ).Normalized();
        Point3f toPrev = ( pos - posPrev ).Normalized();
        Point3f tangent = ( toPrev + toNext ).Normalized();

        float fDot = tangent.Dot( camera.m_direction );
        fDot = fDot * fDot;
        BYTE bAlpha = (BYTE)(int)( fDot * fDot * 255.0f );

        ( (BYTE*)&m_Vertices[i * 2].color )[3]     = bAlpha;
        ( (BYTE*)&m_Vertices[i * 2 + 1].color )[3] = bAlpha;
    }
}




//-----------------------------------------------------------------------------
// Name: ArcClass::Render()
//-----------------------------------------------------------------------------
void ArcClass::Render( IDirect3DDevice9* pd3dDevice, int iDevice )
{
    ARCVERTEX* pVertices = NULL;
    DWORD dwStage;

    m_pVB[iDevice]->Lock( 0, sizeof(m_Vertices), (void**)&pVertices, 0 );
    memcpy( pVertices, m_Vertices, sizeof(m_Vertices) );
    m_pVB[iDevice]->Unlock();

    // Additive blending, depth tested but not written
    pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 1 );
    pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
    pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
    pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
    pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
    pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESS );
    pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    // The same texture on two stages:  diffuse * tex0 * 2 * tex1 * 2
    for( dwStage = 0; dwStage < 2; dwStage++ )
    {
        pd3dDevice->SetTexture( dwStage, m_pTexture[iDevice] );
        pd3dDevice->SetTextureStageState( dwStage, D3DTSS_COLOROP, D3DTOP_MODULATE2X );
        pd3dDevice->SetTextureStageState( dwStage, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        pd3dDevice->SetTextureStageState( dwStage, D3DTSS_COLORARG2, dwStage ? D3DTA_CURRENT : D3DTA_DIFFUSE );
        pd3dDevice->SetTextureStageState( dwStage, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
        pd3dDevice->SetTextureStageState( dwStage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
        pd3dDevice->SetTextureStageState( dwStage, D3DTSS_ALPHAARG2, dwStage ? D3DTA_CURRENT : D3DTA_DIFFUSE );
        pd3dDevice->SetSamplerState( dwStage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        pd3dDevice->SetSamplerState( dwStage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        pd3dDevice->SetSamplerState( dwStage, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
        pd3dDevice->SetSamplerState( dwStage, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
        pd3dDevice->SetSamplerState( dwStage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
        pd3dDevice->SetTextureStageState( dwStage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 );
    }
    pd3dDevice->SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
    pd3dDevice->SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

    // Scroll each layer along the ribbon.  (For 2D texture coordinates the
    // translation lives in the third row of the texture matrix.)
    Matrix44f matTexture;
    matTexture.Identity();
    matTexture.m[2][0] = m_fTexScroll[0];
    pd3dDevice->SetTransform( D3DTS_TEXTURE0, (D3DMATRIX*)&matTexture );
    matTexture.m[2][0] = m_fTexScroll[1];
    pd3dDevice->SetTransform( D3DTS_TEXTURE1, (D3DMATRIX*)&matTexture );

    pd3dDevice->SetVertexShader( NULL );
    pd3dDevice->SetFVF( D3DFVF_ARCVERTEX );
    pd3dDevice->SetStreamSource( 0, m_pVB[iDevice], 0, sizeof(ARCVERTEX) );
    pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, ARC_SEGMENTS * 2 );
}
