//-----------------------------------------------------------------------------
// File: bubbles.cpp
//
// Desc: The Bubbles screen saver: translucent spheres that drift over a
//       snapshot of the desktop.  The spheres are 2D Verlet particles that
//       are pushed around by a slowly changing turbulence field, bounce off
//       the screen edges and are kept apart with a relaxation pass over a
//       uniform grid.  Everything is drawn as pre-transformed quads.
//
//       Original object (14086 bytes of code):
//           shell\osshell\control\scrnsave\bubbles\objfre\amd64\bubbles.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <new>
#include <strsafe.h>
#include <d3d9.h>
#include <d3dx9.h>
#include "d3dsaver9.h"
#include "dxutil9.h"
#include "random.h"
#include "color.h"
#include "noise.h"
#include "texture.h"
#include "bubbles.h"
#include "resource.h"

BubblesScreensaver* g_pMyScreensaver = NULL;

// The simulation advances by a fixed 1/60 s per frame
#define TIME_STEP_SQUARED       ( ( 1.0f / 60.0f ) * ( 1.0f / 60.0f ) )

// Frames between the release of one sphere and the next
#define RELEASE_INTERVAL        20

// How many times per frame the constraints (walls, overlaps) are relaxed
#define RELAXATION_PASSES       3

// The quads are a little larger than the spheres they show
#define SPHERE_QUAD_SCALE       1.175f

// Scale from pixels to noise space
#define TURBULENCE_SCALE        0.002f




//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program.  Note that the (371 KB) screen saver
//       object lives on the stack.
//-----------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow )
{
    HRESULT hr;
    BubblesScreensaver saver;

    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    if( FAILED( hr = saver.Create( hInstance ) ) )
    {
        saver.DisplayErrorMsg( hr );
        return 0;
    }

    return saver.Run();
}




//-----------------------------------------------------------------------------
// Name: BubblesScreensaver()
// Desc: Constructor
//-----------------------------------------------------------------------------
BubblesScreensaver::BubblesScreensaver()
{
    g_pMyScreensaver = this;

    LoadString( NULL, IDS_DESCRIPTION, m_strWindowTitle, 200 );

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = 0;
    InitCommonControlsEx( &icc );

    m_bSpanMultiMon = true;

    ZeroMemory( m_DeviceObjectsArray, sizeof(m_DeviceObjectsArray) );
    m_pDeviceObjects = NULL;
    m_iDevice = 0;

    m_Camera.fFov    = 0.7f;
    m_Camera.fAspect = 1.25f;
    m_Camera.fNear   = 1.0f;
    m_Camera.fFar    = 20.0f;
    m_Camera.eye     = Point3f( 0.0f, 7.0f, 0.0f );
    m_Camera.lookAt  = Point3f( 0.0f, 0.0f, 0.0f );
    m_Camera.up      = Point3f( 0.0f, 0.0f, 1.0f );

    // Defaults; ReadSettings() overrides them from the registry
    m_fSphereDensity        = 0.5f;
    m_nNumSpheres           = BUBBLES_MAX_SPHERES;
    m_fRadius               = 90.0f;
    m_nTurbulenceNumOctaves = 1;
    m_fTurbulenceForce      = 333.0f;
    m_fTurbulenceSpeed      = 0.005f;
    m_bMaterialGlass        = true;
    m_bShowBubbles          = true;
    m_bShowShadows          = true;

    m_bUseDepthBuffer = true;
    m_bAllowRef       = true;
    m_dwMinDepthBits  = 16;
    StringCchCopy( m_strRegPath, 200, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Screensavers\\Bubbles") );

    srand( (UINT)time( NULL ) );

    m_pGrid = NULL;
}




//-----------------------------------------------------------------------------
// Name: OneTimeSceneInit()
// Desc: Called once, after the saver windows and devices exist.  Lines all
//       spheres up in the lower left corner and builds the collision grid.
//-----------------------------------------------------------------------------
HRESULT BubblesScreensaver::OneTimeSceneInit()
{
    HRESULT hr;
    INT i;

    FLOAT fLeft   = (FLOAT)m_rcRenderTotal.left;
    FLOAT fTop    = (FLOAT)m_rcRenderTotal.top;
    FLOAT fRight  = (FLOAT)m_rcRenderTotal.right;
    FLOAT fBottom = (FLOAT)m_rcRenderTotal.bottom;
    FLOAT fWidth  = fRight - fLeft;
    FLOAT fHeight = fBottom - fTop;

    // Everything is a tenth of the size in the little preview window
    if( m_SaverMode == sm_preview )
    {
        m_fRadius *= 0.1f;
        m_fTurbulenceForce *= 0.1f;
    }

    Point2f ptStart( fLeft + m_fRadius, fBottom - m_fRadius );
    FLOAT   fJitter = ( m_SaverMode == sm_preview ) ? 1.0f : 10.0f;

    for( i = 0; i < BUBBLES_MAX_SPHERES; i++ )
    {
        Sphere& sphere = m_Spheres[i];

        // (The shipped code draws the y jitter first.)
        FLOAT fJitterY = RandomFloat();
        FLOAT fJitterX = RandomFloat();

        sphere.m_prevPos = ptStart;
        sphere.m_force   = Point2f( 0.0f, 0.0f );
        sphere.m_pos     = Point2f( fJitterX * fJitter + ptStart.x, -fJitterY * fJitter + ptStart.y );
        sphere.m_fRadius = m_fRadius;
        sphere.nIndex    = i;
        sphere.dwColor   = 0xffffffff;
        sphere.bCollides = false;
    }

    // Cells are as large as a sphere
    hr = Grid2D::Create( &m_pGrid, Point2f( fLeft, fTop ), Point2f( fRight, fBottom ),
                         (INT)( fWidth * 0.5f / m_fRadius ), (INT)( fHeight * 0.5f / m_fRadius ) );
    if( FAILED( hr ) )
        return hr;

    // As many spheres as it takes to cover the wanted fraction of the screen
    // (treating each sphere as the square it is inscribed in)
    m_nNumSpheres = (INT)( fWidth * fHeight * m_fSphereDensity / ( m_fRadius * m_fRadius * 4.0f ) );
    if( m_nNumSpheres < 1 )
        m_nNumSpheres = 1;
    else if( m_nNumSpheres > BUBBLES_MAX_SPHERES )
        m_nNumSpheres = BUBBLES_MAX_SPHERES;

    for( i = 0; i < m_nNumSpheres; i++ )
    {
        m_Spheres[i].m_fRadius = m_fRadius;
        m_pGrid->AddParticle( m_Spheres[i] );
    }

    // FrameMove() releases them one at a time
    m_nNumSpheresTarget = m_nNumSpheres;
    m_nNumSpheres = 1;

    return hr;
}




//-----------------------------------------------------------------------------
// Name: RandomNoisePosition()
// Desc: A random place to start sampling the turbulence field from.  (The
//       shipped code draws the z component first, then y, then x.)
//-----------------------------------------------------------------------------
static inline Point3f RandomNoisePosition()
{
    Point3f pt;

    pt.z = -1000.0f - RandomFloat() * 100.0f;
    pt.y = ( RandomFloat() - RandomFloat() ) * 1000.0f;
    pt.x = ( RandomFloat() - RandomFloat() ) * 1000.0f;
    return pt;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, before any monitor is rendered.
//-----------------------------------------------------------------------------
HRESULT BubblesScreensaver::FrameMove()
{
    INT i;

    //
    // Release the next sphere
    //
    static INT s_nReleaseCountdown = RELEASE_INTERVAL;

    if( m_nNumSpheres < m_nNumSpheresTarget )
    {
        if( --s_nReleaseCountdown < 0 )
        {
            s_nReleaseCountdown = RELEASE_INTERVAL;
            m_nNumSpheres++;
        }
    }

    //
    // Move through the turbulence field.  Two decorrelated samples of the
    // same field give the x and the y component of the force.
    //
    static Point3f s_ptSecondSample( -432.124f, 73.32f, -736.99f );
    static Point3f s_ptNoise = RandomNoisePosition();

    s_ptNoise.z += m_fTurbulenceSpeed;
    if( s_ptNoise.z > 1000.0f )
        s_ptNoise = RandomNoisePosition();

    for( i = 0; i < m_nNumSpheres; i++ )
    {
        Sphere& sphere = m_Spheres[i];
        Point3f ptSample( sphere.m_pos.x * TURBULENCE_SCALE, sphere.m_pos.y * TURBULENCE_SCALE, 0.0f );

        sphere.m_force.x = Turbulence( s_ptNoise + ptSample, m_nTurbulenceNumOctaves, false ) *
                           m_fTurbulenceForce;
        sphere.m_force.y = Turbulence( s_ptNoise + s_ptSecondSample + ptSample, m_nTurbulenceNumOctaves, false ) *
                           m_fTurbulenceForce;

        // Tint the sphere: the hue follows the direction of the force and
        // the saturation its strength.
        FLOAT fStrength = sqrtf( sphere.m_force.x * sphere.m_force.x + sphere.m_force.y * sphere.m_force.y );
        Point2f direction( 0.0f, 0.0f );
        if( fStrength > 0.0001f )
            direction = Point2f( sphere.m_force.x / fStrength, sphere.m_force.y / fStrength );

        FLOAT fSaturation = 1.0f - fStrength / m_fTurbulenceForce;
        fSaturation = 1.0f - fSaturation * fSaturation;

        FLOAT fHue;
        if( direction.y > 0.0f )
            fHue = acosf( direction.x ) / D3DX_PI;
        else
            fHue = 1.0f - acosf( direction.x ) / D3DX_PI;

        FLOAT r, g, b;
        HSLToRGB( fHue, fSaturation, 0.5f, &r, &g, &b );
        sphere.dwColor = ColorToARGB( r, g, b, 1.0f );
    }

    //
    // Verlet integration
    //
    for( i = 0; i < m_nNumSpheres; i++ )
    {
        Sphere& sphere = m_Spheres[i];
        Point2f ptNew( sphere.m_pos.x * 2.0f - sphere.m_prevPos.x + sphere.m_force.x * TIME_STEP_SQUARED,
                       sphere.m_pos.y * 2.0f - sphere.m_prevPos.y + sphere.m_force.y * TIME_STEP_SQUARED );

        sphere.m_prevPos = sphere.m_pos;
        sphere.m_pos = ptNew;
        m_pGrid->MoveParticle( sphere );
    }

    //
    // Satisfy the constraints
    //
    FLOAT fLeft   = (FLOAT)m_rcRenderTotal.left;
    FLOAT fRight  = (FLOAT)m_rcRenderTotal.right;
    FLOAT fTop    = (FLOAT)m_rcRenderTotal.top;
    FLOAT fBottom = (FLOAT)m_rcRenderTotal.bottom;

    for( INT iPass = 0; iPass < RELAXATION_PASSES; iPass++ )
    {
        for( i = 0; i < m_nNumSpheres; i++ )
        {
            Sphere& sphere = m_Spheres[i];

            // Bounce off the edges of the screen by mirroring both the
            // current and the previous position
            if( sphere.m_pos.x < fLeft )
            {
                sphere.m_pos.x     = fLeft * 2.0f - sphere.m_pos.x;
                sphere.m_prevPos.x = fLeft * 2.0f - sphere.m_prevPos.x;
                m_pGrid->MoveParticle( sphere );
            }
            else if( sphere.m_pos.x > fRight )
            {
                sphere.m_pos.x     = fRight * 2.0f - sphere.m_pos.x;
                sphere.m_prevPos.x = fRight * 2.0f - sphere.m_prevPos.x;
                m_pGrid->MoveParticle( sphere );
            }

            if( sphere.m_pos.y < fTop )
            {
                sphere.m_pos.y     = fTop * 2.0f - sphere.m_pos.y;
                sphere.m_prevPos.y = fTop * 2.0f - sphere.m_prevPos.y;
                m_pGrid->MoveParticle( sphere );
            }
            else if( sphere.m_pos.y > fBottom )
            {
                sphere.m_pos.y     = fBottom * 2.0f - sphere.m_pos.y;
                sphere.m_prevPos.y = fBottom * 2.0f - sphere.m_prevPos.y;
                m_pGrid->MoveParticle( sphere );
            }

            // The grid cells that a sphere at this position can touch
            FLOAT   fDiameter = sphere.m_fRadius * 2.0f;
            Point2f pos = sphere.m_pos;
            INT     nMinX, nMinY, nMaxX, nMaxY;
            INT     x, y;

            m_pGrid->MapPoint( Point2f( pos.x - fDiameter, pos.y - fDiameter ), nMinX, nMinY );
            m_pGrid->MapPoint( Point2f( pos.x + fDiameter, pos.y + fDiameter ), nMaxX, nMaxY );

            if( !sphere.bCollides )
            {
                // All spheres are released at the same spot.  A new one is
                // ignored by the others until it has drifted clear of them.
                bool bOverlaps = false;

                for( x = nMinX; x <= nMaxX; x++ )
                {
                    for( y = nMinY; y <= nMaxY; y++ )
                    {
                        for( Particle2D* pOther = m_pGrid->Cell( x, y ).m_pHead;
                             pOther != NULL && !bOverlaps;
                             pOther = pOther->m_pNext )
                        {
                            if( i != ( (Sphere*)pOther )->nIndex )
                            {
                                FLOAT dy = sphere.m_pos.y - pOther->m_pos.y;
                                FLOAT dx = sphere.m_pos.x - pOther->m_pos.x;
                                if( sqrtf( dy * dy + dx * dx ) < fDiameter )
                                    bOverlaps = true;
                            }
                        }
                    }
                }

                if( !bOverlaps )
                    sphere.bCollides = true;
            }
            else
            {
                // Push overlapping spheres apart.  Every pair is handled
                // once, by the sphere with the lower index.
                for( x = nMinX; x <= nMaxX; x++ )
                {
                    for( y = nMinY; y <= nMaxY; y++ )
                    {
                        for( Particle2D* pNode = m_pGrid->Cell( x, y ).m_pHead;
                             pNode != NULL;
                             pNode = pNode->m_pNext )
                        {
                            INT j = ( (Sphere*)pNode )->nIndex;

                            if( i < j && m_Spheres[j].bCollides )
                            {
                                Sphere& other = m_Spheres[j];
                                FLOAT dx = sphere.m_pos.x - other.m_pos.x;
                                FLOAT dy = sphere.m_pos.y - other.m_pos.y;
                                FLOAT fDistance = sqrtf( dy * dy + dx * dx );

                                if( fDistance < fDiameter )
                                {
                                    Point2f push( ( fDiameter - fDistance ) * ( dx / fDistance ),
                                                  ( dy / fDistance ) * ( fDiameter - fDistance ) );

                                    sphere.m_pos.x += push.x;
                                    sphere.m_pos.y += push.y;
                                    other.m_pos.x  -= push.x;
                                    other.m_pos.y  -= push.y;

                                    m_pGrid->MoveParticle( sphere );
                                    m_pGrid->MoveParticle( other );
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetSphereQuad()
// Desc: Corners of the quad of a sphere (clockwise from the top right).
//       (Expanded inline three times in the shipped image.)
//-----------------------------------------------------------------------------
static inline void SetSphereQuad( SPHEREVERTEX* pVertices, const Sphere& sphere, FLOAT fOffsetX, FLOAT fOffsetY )
{
    FLOAT fSize = sphere.m_fRadius * SPHERE_QUAD_SCALE;

    pVertices[0].x = sphere.m_pos.x + fSize *  1.0f + fOffsetX;
    pVertices[0].y = sphere.m_pos.y + fSize * -1.0f + fOffsetY;
    pVertices[0].z = 1.0f;
    pVertices[1].x = sphere.m_pos.x + fSize * -1.0f + fOffsetX;
    pVertices[1].y = sphere.m_pos.y + fSize * -1.0f + fOffsetY;
    pVertices[1].z = 1.0f;
    pVertices[2].x = sphere.m_pos.x + fSize * -1.0f + fOffsetX;
    pVertices[2].y = sphere.m_pos.y + fSize *  1.0f + fOffsetY;
    pVertices[2].z = 1.0f;
    pVertices[3].x = sphere.m_pos.x + fSize *  1.0f + fOffsetX;
    pVertices[3].y = sphere.m_pos.y + fSize *  1.0f + fOffsetY;
    pVertices[3].z = 1.0f;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame for every render unit (monitor).
//-----------------------------------------------------------------------------
HRESULT BubblesScreensaver::Render()
{
    D3DVIEWPORT9 vp;
    RECT rcTotal;
    RECT rcVisible;
    INT i;

    // First, clear the entire back buffer to the background color
    vp.X      = 0;
    vp.Y      = 0;
    vp.Width  = m_rcRenderCurDevice.right - m_rcRenderCurDevice.left;
    vp.Height = m_rcRenderCurDevice.bottom - m_rcRenderCurDevice.top;
    vp.MinZ   = 0.0f;
    vp.MaxZ   = 1.0f;
    m_pd3dDevice->SetViewport( &vp );
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0L );

    // Now determine what part of the total render area this device shows
    rcTotal = m_rcRenderTotal;
    if( !IntersectRect( &rcVisible, &rcTotal, &m_rcRenderCurDevice ) )
        return S_OK;

    OffsetRect( &rcTotal, -m_rcRenderCurDevice.left, -m_rcRenderCurDevice.top );
    OffsetRect( &rcVisible, -m_rcRenderCurDevice.left, -m_rcRenderCurDevice.top );

    vp.X      = rcVisible.left;
    vp.Y      = rcVisible.top;
    vp.Width  = rcVisible.right - rcVisible.left;
    vp.Height = rcVisible.bottom - rcVisible.top;
    vp.MinZ   = 0.0f;
    vp.MaxZ   = 1.0f;
    m_pd3dDevice->SetViewport( &vp );

    FLOAT fTotalWidth  = (FLOAT)( m_rcRenderTotal.right - m_rcRenderTotal.left );
    FLOAT fTotalHeight = (FLOAT)( m_rcRenderTotal.bottom - m_rcRenderTotal.top );

    if( m_bSpanMultiMon )
        m_Camera.fAspect = fTotalWidth / fTotalHeight;
    else
        m_Camera.fAspect = (FLOAT)vp.Width / (FLOAT)vp.Height;

    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovRH( &matProj, m_Camera.fFov, m_Camera.fAspect, m_Camera.fNear, m_Camera.fFar );

    if( m_bSpanMultiMon )
    {
        // The projection above spans all monitors.  Scale and shift it so
        // that this device ends up showing just its own piece of that view.
        FLOAT fDeviceWidth  = (FLOAT)( m_rcRenderCurDevice.right - m_rcRenderCurDevice.left );
        FLOAT fDeviceHeight = (FLOAT)( m_rcRenderCurDevice.bottom - m_rcRenderCurDevice.top );
        FLOAT fScaleX = 2.0f / fDeviceWidth;
        FLOAT fScaleY = 2.0f / fDeviceHeight;

        matProj._11 *= fTotalWidth / fDeviceWidth;
        matProj._22 *= fTotalHeight / fDeviceHeight;
        matProj._31 = (FLOAT)( m_rcRenderCurDevice.left + m_rcRenderCurDevice.right ) * 0.5f * fScaleX -
                      (FLOAT)( m_rcRenderTotal.left + m_rcRenderTotal.right ) * 0.5f * fScaleX;
        matProj._32 = (FLOAT)( m_rcRenderTotal.top + m_rcRenderTotal.bottom ) * 0.5f * fScaleY -
                      (FLOAT)( m_rcRenderCurDevice.top + m_rcRenderCurDevice.bottom ) * 0.5f * fScaleY;
    }
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    m_pd3dDevice->BeginScene();

    D3DXMATRIX matIdentity;
    D3DXMatrixIdentity( &matIdentity );
    m_matWorld = matIdentity;
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matWorld );

    D3DXMatrixLookAtRH( &m_matView, (D3DXVECTOR3*)&m_Camera.eye, (D3DXVECTOR3*)&m_Camera.lookAt,
                        (D3DXVECTOR3*)&m_Camera.up );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );

    SetPerFrameStates();

    // One quad, reused for everything that follows.  It starts out covering
    // the back buffer.
    SPHEREVERTEX quad[4];
    FLOAT fQuadRight  = (FLOAT)vp.Width + (FLOAT)vp.X - 0.5f;
    FLOAT fQuadBottom = (FLOAT)vp.Height + (FLOAT)vp.Y - 0.5f;

    quad[0].x = -0.5f;       quad[0].y = -0.5f;        quad[0].z = 1.0f;  quad[0].rhw = 1.0f;
    quad[1].x = fQuadRight;  quad[1].y = -0.5f;        quad[1].z = 1.0f;  quad[1].rhw = 1.0f;
    quad[2].x = fQuadRight;  quad[2].y = fQuadBottom;  quad[2].z = 1.0f;  quad[2].rhw = 1.0f;
    quad[3].x = -0.5f;       quad[3].y = fQuadBottom;  quad[3].z = 1.0f;  quad[3].rhw = 1.0f;
    quad[0].color = quad[1].color = quad[2].color = quad[3].color = 0xffffffff;

    //
    // The desktop as it was when the screen saver started
    //
    if( m_bShowBubbles && m_pDeviceObjects->pDesktopTexture != NULL )
    {
        // The texture may have been rounded up to a power of two
        D3DSURFACE_DESC desc;
        m_pDeviceObjects->pDesktopTexture->GetLevelDesc( 0, &desc );
        FLOAT fMaxU = (FLOAT)m_pDeviceObjects->nDesktopWidth / (FLOAT)desc.Width;
        FLOAT fMaxV = (FLOAT)m_pDeviceObjects->nDesktopHeight / (FLOAT)desc.Height;

        quad[0].tu = 0.0f;   quad[0].tv = 0.0f;
        quad[1].tu = fMaxU;  quad[1].tv = 0.0f;
        quad[2].tu = fMaxU;  quad[2].tv = fMaxV;
        quad[3].tu = 0.0f;   quad[3].tv = fMaxV;

        m_pd3dDevice->SetTexture( 0, m_pDeviceObjects->pDesktopTexture );
        m_pd3dDevice->SetTexture( 1, NULL );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
        m_pd3dDevice->SetFVF( D3DFVF_SPHEREVERTEX );
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, quad, sizeof(SPHEREVERTEX) );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    }

    // From here on the quad shows a whole texture, top right corner first
    quad[0].tu = 1.0f;  quad[0].tv = 0.0f;
    quad[1].tu = 0.0f;  quad[1].tv = 0.0f;
    quad[2].tu = 0.0f;  quad[2].tv = 1.0f;
    quad[3].tu = 1.0f;  quad[3].tv = 1.0f;

    //
    // Shadows (only on top of the desktop)
    //
    if( m_bShowShadows && m_bShowBubbles )
    {
        // Glass casts a much fainter shadow
        if( m_bMaterialGlass )
        {
            for( i = 0; i < 4; i++ )
                quad[i].color = ( quad[i].color & 0x00ffffff ) | 0x1e000000;
        }

        m_pd3dDevice->SetTexture( 0, m_pDeviceObjects->pShadowTexture );
        m_pd3dDevice->SetTexture( 1, NULL );
        m_pd3dDevice->SetTexture( 2, NULL );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2 );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TEXTURE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
        m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
        m_pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
        m_pd3dDevice->SetTextureStageState( 2, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );

        // The shadow falls a little to the right of and below its sphere
        FLOAT fOffsetY = -(FLOAT)m_rcRenderCurDevice.top + m_fRadius * 0.4f;
        FLOAT fOffsetX = -(FLOAT)m_rcRenderCurDevice.left + m_fRadius * 0.18f;

        for( i = 0; i < m_nNumSpheres; i++ )
        {
            SetSphereQuad( quad, m_Spheres[i], fOffsetX, fOffsetY );
            m_pd3dDevice->SetFVF( D3DFVF_SPHEREVERTEX );
            m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, quad, sizeof(SPHEREVERTEX) );
        }

        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    }

    //
    // The spheres:  diffuse * material + highlights, premultiplied alpha
    //
    if( !m_bMaterialGlass )
        m_pd3dDevice->SetTexture( 0, m_pDeviceObjects->pSolidTexture );
    else
        m_pd3dDevice->SetTexture( 0, m_pDeviceObjects->pGlassTexture );
    m_pd3dDevice->SetTexture( 1, m_pDeviceObjects->pHighlightTexture );
    m_pd3dDevice->SetTexture( 2, NULL );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 0 );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_ADD );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_CURRENT );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_CURRENT );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG2, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );

    {
        FLOAT fOffsetX = -(FLOAT)m_rcRenderCurDevice.left;
        FLOAT fOffsetY = -(FLOAT)m_rcRenderCurDevice.top;

        for( i = 0; i < m_nNumSpheres; i++ )
        {
            SetSphereQuad( quad, m_Spheres[i], fOffsetX, fOffsetY );
            quad[0].color = quad[1].color = quad[2].color = quad[3].color = m_Spheres[i].dwColor;
            m_pd3dDevice->SetFVF( D3DFVF_SPHEREVERTEX );
            m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, quad, sizeof(SPHEREVERTEX) );
        }
    }

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    m_pd3dDevice->EndScene();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc: Called for every render unit when its device is created or reset.
//-----------------------------------------------------------------------------
HRESULT BubblesScreensaver::RestoreDeviceObjects()
{
    HRESULT hr = E_FAIL;

    D3DXMatrixLookAtRH( &m_matView, (D3DXVECTOR3*)&m_Camera.eye, (D3DXVECTOR3*)&m_Camera.lookAt,
                        (D3DXVECTOR3*)&m_Camera.up );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    if( m_pDeviceObjects->pShadowTexture == NULL )
    {
        hr = Texture::LoadTextureFromResource( m_pd3dDevice, MAKEINTRESOURCEW( IDR_SHADOW ), L"BMP",
                                               &m_pDeviceObjects->pShadowTexture );
        if( FAILED( hr ) )
            return hr;
    }

    if( m_pDeviceObjects->pSolidTexture == NULL )
    {
        hr = Texture::LoadTextureFromResource( m_pd3dDevice, MAKEINTRESOURCEW( IDR_SPHERE_SOLID ), L"BMP",
                                               &m_pDeviceObjects->pSolidTexture );
        if( FAILED( hr ) )
        {
            SAFE_RELEASE( m_pDeviceObjects->pShadowTexture );
            return hr;
        }
    }

    if( m_pDeviceObjects->pHighlightTexture == NULL )
    {
        hr = Texture::LoadTextureFromResource( m_pd3dDevice, MAKEINTRESOURCEW( IDR_SPHERE_HIGHLIGHT ), L"BMP",
                                               &m_pDeviceObjects->pHighlightTexture );
        if( FAILED( hr ) )
        {
            SAFE_RELEASE( m_pDeviceObjects->pShadowTexture );
            SAFE_RELEASE( m_pDeviceObjects->pSolidTexture );
            return hr;
        }
    }

    if( m_pDeviceObjects->pGlassTexture == NULL )
    {
        hr = Texture::LoadTextureFromResource( m_pd3dDevice, MAKEINTRESOURCEW( IDR_SPHERE_GLASS ), L"BMP",
                                               &m_pDeviceObjects->pGlassTexture );
        if( FAILED( hr ) )
        {
            SAFE_RELEASE( m_pDeviceObjects->pShadowTexture );
            SAFE_RELEASE( m_pDeviceObjects->pSolidTexture );
            SAFE_RELEASE( m_pDeviceObjects->pHighlightTexture );
            return hr;
        }
    }

    // Turn the desktop snapshot of this monitor into a texture
    if( g_pMyScreensaver->m_bShowBubbles )
    {
        if( m_pDeviceObjects->pDesktopImage != NULL )
        {
            hr = Texture::CreateTextureFromTGAFileInMemory(
                     m_pd3dDevice, m_pDeviceObjects->pDesktopImage,
                     m_pDeviceObjects->nDesktopHeight * m_pDeviceObjects->nDesktopWidth * 4 + 18,
                     0, D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8, &m_pDeviceObjects->pDesktopTexture );
        }
        if( m_pDeviceObjects->pDesktopImage == NULL || FAILED( hr ) )
            m_pDeviceObjects->pDesktopTexture = NULL;
    }

    SetPerFrameStates();

    m_dwLastTick = GetTickCount();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetPerFrameStates()
// Desc: Render states everything in Render() starts from.
//-----------------------------------------------------------------------------
VOID BubblesScreensaver::SetPerFrameStates()
{
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, FALSE );

    // Premultiplied alpha, and nothing at all where the texture is transparent
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 1 );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
}




//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
// Desc: Called for every render unit before its device is reset or destroyed.
//-----------------------------------------------------------------------------
HRESULT BubblesScreensaver::InvalidateDeviceObjects()
{
    SAFE_RELEASE( m_pDeviceObjects->pShadowTexture );
    SAFE_RELEASE( m_pDeviceObjects->pSolidTexture );
    SAFE_RELEASE( m_pDeviceObjects->pGlassTexture );
    SAFE_RELEASE( m_pDeviceObjects->pHighlightTexture );
    SAFE_RELEASE( m_pDeviceObjects->pDesktopTexture );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetDevice()
// Desc: The framework is about to call us for another render unit.
//-----------------------------------------------------------------------------
VOID BubblesScreensaver::SetDevice( UINT iDevice )
{
    m_iDevice = iDevice;
    m_pDeviceObjects = &m_DeviceObjectsArray[iDevice];
}




//-----------------------------------------------------------------------------
// Name: MonitorEnumProc()
// Desc: Takes a snapshot of one monitor.  lParam is non-zero in preview mode.
//
//       The pixels are stored behind an 18-byte Targa header, so that
//       Texture::CreateTextureFromTGAFileInMemory() can be reused to upload
//       them.  GetDIBits() hands back a bottom-up DIB, which happens to be
//       the Targa default as well.
//-----------------------------------------------------------------------------
BOOL CALLBACK MonitorEnumProc( HMONITOR hMonitor, HDC hdc, LPRECT prcMonitor, LPARAM lParam )
{
    (void)hMonitor;

    if( g_pMyScreensaver->m_bShowBubbles )
    {
        // Let the Screen Saver Settings dialog finish painting first
        if( lParam )
            Sleep( 500 );

        INT nWidth  = prcMonitor->right - prcMonitor->left;
        INT nHeight = prcMonitor->bottom - prcMonitor->top;

        g_pMyScreensaver->m_pDeviceObjects->nDesktopWidth  = nWidth;
        g_pMyScreensaver->m_pDeviceObjects->nDesktopHeight = nHeight;
        g_pMyScreensaver->m_pDeviceObjects->pDesktopImage  = NULL;

        HDC hdcMem = CreateCompatibleDC( hdc );
        if( hdcMem != NULL )
        {
            HBITMAP hbm = CreateBitmap( nWidth, nHeight, 1, 32, NULL );
            if( hbm != NULL )
            {
                HGDIOBJ hbmOld = SelectObject( hdcMem, hbm );

                // Ask for the format of the bitmap
                BITMAPINFO bmi;
                ZeroMemory( &bmi, sizeof(bmi) );
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biBitCount = 0;
                GetDIBits( hdcMem, hbm, 0, nHeight, NULL, &bmi, DIB_RGB_COLORS );

                INT cbBits = bmi.bmiHeader.biBitCount * bmi.bmiHeader.biHeight * bmi.bmiHeader.biWidth / 8;

                g_pMyScreensaver->m_pDeviceObjects->pDesktopImage = new( std::nothrow ) BYTE[cbBits + 18];
                if( g_pMyScreensaver->m_pDeviceObjects->pDesktopImage != NULL )
                {
                    GetDIBits( hdcMem, hbm, 0, nHeight, NULL, &bmi, DIB_RGB_COLORS );

                    BitBlt( hdcMem, 0, 0, nWidth, nHeight, hdc, prcMonitor->left, prcMonitor->top,
                            SRCCOPY | CAPTUREBLT );

                    GetDIBits( hdcMem, hbm, 0, nHeight,
                               g_pMyScreensaver->m_pDeviceObjects->pDesktopImage + 18, &bmi, DIB_RGB_COLORS );

                    // Targa header: uncompressed true-colour, 32 bits per
                    // pixel, bottom-up.  (The two 32-bit stores overlap each
                    // other and the bytes that are written after them, just
                    // like in the shipped code.)
                    BYTE* pHeader = g_pMyScreensaver->m_pDeviceObjects->pDesktopImage;
                    ZeroMemory( pHeader, 18 );
                    pHeader[2] = 2;
                    memcpy( pHeader + 12, &nWidth, sizeof(INT) );
                    memcpy( pHeader + 14, &nHeight, sizeof(INT) );
                    pHeader[16] = 32;
                    pHeader[17] = 0;
                }

                SelectObject( hdcMem, hbmOld );
                DeleteObject( hbm );
            }
            DeleteDC( hdcMem );
        }
    }

    // The next monitor fills the next slot
    g_pMyScreensaver->m_pDeviceObjects++;

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: FullModeHack()
// Desc: Called by CD3DScreensaver::Create() before any saver window has been
//       created, which makes it the last chance to see the desktop: grab a
//       snapshot of every monitor.
//
//       (sic) The snapshots are stored in monitor enumeration order, but are
//       later used by render unit index, and the device context is never
//       released.
//-----------------------------------------------------------------------------
VOID BubblesScreensaver::FullModeHack()
{
    m_pDeviceObjects = &m_DeviceObjectsArray[0];

    EnumDisplayMonitors( GetDC( NULL ), NULL, MonitorEnumProc, (LPARAM)( m_SaverMode == sm_preview ) );
}
