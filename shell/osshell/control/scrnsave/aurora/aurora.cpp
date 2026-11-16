//-----------------------------------------------------------------------------
// File: aurora.cpp
//
// Desc: The Aurora screen saver: up to ten translucent ribbons of light that
//       are bent by 3D noise, seen by a slowly swaying camera.
//
//       Original object (3628 bytes of code):
//           shell\osshell\control\scrnsave\aurora\objfre\amd64\aurora.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <time.h>
#include <strsafe.h>
#include <d3d9.h>
#include <d3dx9.h>
#include "d3dsaver9.h"
#include "aurora.h"
#include "resource.h"

AuroraScreensaver* g_pMyScreensaver = NULL;




//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program.  Note that the (142 KB) screen saver
//       object lives on the stack.
//-----------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow )
{
    HRESULT hr;

    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    HeapSetInformation( NULL, HeapEnableTerminationOnCorruption, NULL, 0 );

    AuroraScreensaver saver;

    if( FAILED( hr = saver.Create( hInstance ) ) )
    {
        saver.DisplayErrorMsg( hr );
        return 0;
    }

    return saver.Run();
}




//-----------------------------------------------------------------------------
// Name: AuroraScreensaver()
// Desc: Constructor
//-----------------------------------------------------------------------------
AuroraScreensaver::AuroraScreensaver()
{
    g_pMyScreensaver = this;

    LoadString( NULL, IDS_DESCRIPTION, m_strWindowTitle, 200 );

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = 0;
    InitCommonControlsEx( &icc );

    ZeroMemory( m_DeviceObjectsArray, sizeof(m_DeviceObjectsArray) );
    m_iDevice = 0;

    // Defaults; ReadSettings() overrides them from the registry
    m_bSpanMultiMon = true;
    m_nNumLayers    = 5;
    m_fBrightness   = 0.075f;
    m_fSpeed        = 0.0002f;
    m_fAmplitude    = 2.0f;

    m_bUseDepthBuffer = true;
    m_dwMinDepthBits  = 16;
    StringCchCopy( m_strRegPath, 200, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Screensavers\\Aurora") );

    srand( (UINT)time( NULL ) );
}




//-----------------------------------------------------------------------------
// Name: ConfirmDevice()
// Desc: Any device will do: Aurora only needs the fixed-function pipeline.
//-----------------------------------------------------------------------------
HRESULT AuroraScreensaver::ConfirmDevice( D3DCAPS9* pCaps, DWORD dwBehavior, D3DFORMAT fmtBackBuffer )
{
    (void)pCaps;
    (void)dwBehavior;
    (void)fmtBackBuffer;
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, before any monitor is rendered.
//-----------------------------------------------------------------------------
HRESULT AuroraScreensaver::FrameMove()
{
    m_dwLastTick = GetTickCount();

    m_Camera.Update();

    for( INT i = 0; i < m_nNumLayers; i++ )
        m_Arcs[i].Update( m_Camera, m_fSpeed, m_fBrightness, m_fAmplitude );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame for every render unit (monitor).
//-----------------------------------------------------------------------------
HRESULT AuroraScreensaver::Render()
{
    D3DVIEWPORT9 vp;
    RECT rcTotal;
    RECT rcVisible;

    // First, clear the entire back buffer to the background color
    vp.X      = 0;
    vp.Y      = 0;
    vp.Width  = m_rcRenderCurDevice.right - m_rcRenderCurDevice.left;
    vp.Height = m_rcRenderCurDevice.bottom - m_rcRenderCurDevice.top;
    vp.MinZ   = 0.0f;
    vp.MaxZ   = 1.0f;
    m_pd3dDevice->SetViewport( &vp );

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
        m_Camera.m_fAspect = fTotalWidth / fTotalHeight;
    else
        m_Camera.m_fAspect = (FLOAT)vp.Width / (FLOAT)vp.Height;

    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovRH( &matProj, m_Camera.m_fFov, m_Camera.m_fAspect,
                                m_Camera.m_fNear, m_Camera.m_fFar );

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

    D3DXMATRIX matIdentity;
    D3DXMatrixIdentity( &matIdentity );
    m_matWorld = matIdentity;
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matWorld );

    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1.0f, 0L );

    m_pd3dDevice->BeginScene();

    D3DXMATRIX matView;
    D3DXMatrixLookAtRH( &matView, (D3DXVECTOR3*)&m_Camera.m_eye, (D3DXVECTOR3*)&m_Camera.m_lookAt,
                        (D3DXVECTOR3*)&m_Camera.m_up );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    for( INT i = 0; i < m_nNumLayers; i++ )
        m_Arcs[i].Render( m_pd3dDevice, m_iDevice );

    m_pd3dDevice->EndScene();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: NextPowerOfTwo()
// Desc: (expanded inline in the shipped image)
//-----------------------------------------------------------------------------
static inline UINT NextPowerOfTwo( UINT u )
{
    if( u & ( u - 1 ) )
    {
        u *= 2;
        while( u & ( u - 1 ) )
            u &= u - 1;
    }
    return u;
}




//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc: Called for every render unit when its device is created or reset.
//-----------------------------------------------------------------------------
HRESULT AuroraScreensaver::RestoreDeviceObjects()
{
    HRESULT hr;

    // Placeholder transforms; Render() sets the real ones every frame
    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovRH( &matProj, 1.0f, 4.0f / 3.0f, 0.1f, 300.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    D3DXVECTOR3 vEye( 0.0f, 0.0f, -10.0f );
    D3DXVECTOR3 vAt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );
    D3DXMATRIX matView;
    D3DXMatrixLookAtRH( &matView, &vEye, &vAt, &vUp );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    D3DVIEWPORT9 vp;
    m_pd3dDevice->GetViewport( &vp );
    UINT uWidth  = vp.X + vp.Width;
    UINT uHeight = vp.Y + vp.Height;

    if( m_DeviceObjectsArray[m_iDevice].pRenderTexture == NULL )
    {
        hr = m_pd3dDevice->CreateTexture( NextPowerOfTwo( uWidth ), NextPowerOfTwo( uHeight ), 1,
                                          D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                          &m_DeviceObjectsArray[m_iDevice].pRenderTexture, NULL );
        if( FAILED( hr ) )
        {
            m_DeviceObjectsArray[m_iDevice].pRenderTexture = NULL;
            return hr;
        }
    }

    if( m_DeviceObjectsArray[m_iDevice].pDepthStencil == NULL )
    {
        hr = m_pd3dDevice->CreateDepthStencilSurface( uWidth, uHeight, D3DFMT_D24S8,
                                                      D3DMULTISAMPLE_NONE, 0, TRUE,
                                                      &m_DeviceObjectsArray[m_iDevice].pDepthStencil, NULL );
        if( FAILED( hr ) )
        {
            m_DeviceObjectsArray[m_iDevice].pRenderTexture = NULL;
            return hr;
        }
    }

    for( INT i = 0; i < m_nNumLayers; i++ )
        m_Arcs[i].InitDevice( m_pd3dDevice, m_iDevice, m_fBrightness );

    m_dwLastTick = GetTickCount();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
// Desc: Called for every render unit before its device is reset or destroyed.
//-----------------------------------------------------------------------------
HRESULT AuroraScreensaver::InvalidateDeviceObjects()
{
    if( m_pDeviceObjects->pRenderTexture != NULL )
    {
        m_pDeviceObjects->pRenderTexture->Release();
        m_pDeviceObjects->pRenderTexture = NULL;
    }
    if( m_pDeviceObjects->pDepthStencil != NULL )
    {
        m_pDeviceObjects->pDepthStencil->Release();
        m_pDeviceObjects->pDepthStencil = NULL;
    }

    for( INT i = 0; i < m_nNumLayers; i++ )
        m_Arcs[i].InvalidateDevice( m_iDevice );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetDevice()
// Desc: The framework is about to call us for another render unit.
//-----------------------------------------------------------------------------
VOID AuroraScreensaver::SetDevice( UINT iDevice )
{
    m_iDevice = iDevice;
    m_pDeviceObjects = &m_DeviceObjectsArray[iDevice];
}
