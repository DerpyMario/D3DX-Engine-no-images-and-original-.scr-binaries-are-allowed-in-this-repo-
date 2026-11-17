//-----------------------------------------------------------------------------
// File: ssbranded.cpp
//
// Desc: The Windows Energy screen saver - see ssbranded.h.
//
//       The show is a little state machine (vars.h): the scenes post
//       messages (g_eMsg) when they are done, FrameMove() acts on them and
//       moves the scene of the current state (g_eState), and Render() draws
//       it.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\ssbranded.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <commctrl.h>
#include <new>
#include "ssbranded.h"
#include "texture.h"
#include "resource.h"

// The wipes between the scenes.  The scenes are filled in when a wipe starts.
CROSSWIPEPARMS CSSBranded::s_asCWParms[SSBRANDED_WIPES] =
{
    // logo -> Microsoft flare
    { 3.0f, 1.0f,  IDR_TGA_CROSSWIPE, NULL, NULL, D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ), D3DXCOLOR( 0.0f, 1.0f, 0.0f, 1.0f ),
      SSMSG_LOGO_TO_MSFLARE_DONE },
    // Microsoft flare -> flare
    { 1.0f, 1.0f,  IDR_TGA_CROSSWIPE, NULL, NULL, D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ), D3DXCOLOR( 0.0f, 1.0f, 0.0f, 1.0f ),
      SSMSG_MSFLARE_TO_FLARE_DONE },
    // flare -> black (the logo scene starts on black)
    { 4.0f, 0.75f, IDR_TGA_CROSSWIPE, NULL, NULL, D3DXCOLOR( 0.0f, 0.0f, 1.0f, 1.0f ), D3DXCOLOR( 0.0f, 0.0f, 0.0f, 1.0f ),
      SSMSG_FLARE_TO_LOGO_DONE },
};




//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, INT /*nCmdShow*/ )
{
    HeapSetInformation( NULL, HeapEnableTerminationOnCorruption, NULL, 0 );

    CSSBranded* pSaver = new( std::nothrow ) CSSBranded;
    if( pSaver != NULL )
    {
        pSaver->Create( hInstance );

        if( SUCCEEDED( D3DXCreateMatrixStack( 0, &g_pViewStack ) ) &&
            SUCCEEDED( D3DXCreateMatrixStack( 0, &g_pProjStack ) ) )
        {
            g_eState = SS_START;
        }

        pSaver->Run();

        delete pSaver;
    }

    return 0;
}




//-----------------------------------------------------------------------------
// Name: MSLoadTextureFromResource()
// Desc: Creates a texture (with automatically generated mip maps) from a
//       Targa image in a resource of the executable.
//-----------------------------------------------------------------------------
HRESULT MSLoadTextureFromResource( IDirect3DDevice9* pd3dDevice, LPCWSTR pszName, LPCWSTR pszType,
                                   IDirect3DTexture9** ppTexture )
{
    HRESULT hr = S_OK;

    *ppTexture = NULL;

    HRSRC hResInfo = FindResourceW( NULL, pszName, pszType );
    if( hResInfo != NULL )
    {
        DWORD cbData = SizeofResource( NULL, hResInfo );
        if( cbData != 0 )
        {
            HGLOBAL hResData = LoadResource( NULL, hResInfo );
            if( hResData != NULL )
            {
                BYTE* pData = (BYTE*)LockResource( hResData );
                if( pData != NULL )
                {
                    hr = Texture::CreateTextureFromTGAFileInMemory( pd3dDevice, pData, cbData, 0,
                                                                    D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8,
                                                                    ppTexture );
                    if( FAILED( hr ) )
                        *ppTexture = NULL;
                }
            }
        }
    }

    if( FAILED( hr ) || *ppTexture == NULL )
        return D3DAPPERR_MEDIANOTFOUND;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CSSBranded()
//-----------------------------------------------------------------------------
CSSBranded::CSSBranded() :
    m_bDeviceObjectsRestored( false )
{
    m_bUseDepthBuffer = true;
    m_bOneScreenOnly = true;
}




//-----------------------------------------------------------------------------
// Name: DoConfig()
//-----------------------------------------------------------------------------
VOID CSSBranded::DoConfig()
{
    TaskDialog( m_hWndParent, NULL,
                MAKEINTRESOURCEW( IDS_CONFIG_TITLE ),
                MAKEINTRESOURCEW( IDS_CONFIG_NOOPTIONS ),
                MAKEINTRESOURCEW( IDS_CONFIG_NOOPTIONS_TEXT ),
                TDCBF_OK_BUTTON, TD_INFORMATION_ICON, NULL );
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Acts on the message of the last frame, then moves the scene of the
//       current state, on every device.
//-----------------------------------------------------------------------------
HRESULT CSSBranded::FrameMove()
{
    UINT i;

    switch( g_eMsg )
    {
        case SSMSG_LOGO_DONE:
            for( i = 0; i < m_dwNumRenderUnits; i++ )
            {
                SetDevice( i );
                m_aViews[i].m_pMSFlareScene->Go( m_fTime );
                s_asCWParms[0].pSrc0 = m_aViews[i].m_pLogoScene;
                s_asCWParms[0].pSrc1 = m_aViews[i].m_pMSFlareScene;
                m_aViews[i].m_pLogoToMSFlare->Setup( &s_asCWParms[0] );
                m_aViews[i].m_pLogoToMSFlare->Go( m_fTime );
            }
            g_eState = SS_LOGO_TO_MSFLARE;
            break;

        case SSMSG_LOGO_TO_MSFLARE_DONE:
            for( i = 0; i < m_dwNumRenderUnits; i++ )
            {
                SetDevice( i );
                m_aViews[i].m_pLogoScene->Stop( m_fTime );
                m_aViews[i].m_pLogoToMSFlare->Stop( m_fTime );
            }
            g_eState = SS_MSFLARE;
            break;

        case SSMSG_MSFLARE_DONE:
            for( i = 0; i < m_dwNumRenderUnits; i++ )
            {
                SetDevice( i );
                m_aViews[i].m_pFlareScene->Go( m_fTime );
                s_asCWParms[1].pSrc0 = m_aViews[i].m_pMSFlareScene;
                s_asCWParms[1].pSrc1 = m_aViews[i].m_pFlareScene;
                m_aViews[i].m_pMSFlareToFlare->Setup( &s_asCWParms[1] );
                m_aViews[i].m_pMSFlareToFlare->Go( m_fTime );
            }
            g_eState = SS_MSFLARE_TO_FLARE;
            break;

        case SSMSG_MSFLARE_TO_FLARE_DONE:
            for( i = 0; i < m_dwNumRenderUnits; i++ )
            {
                SetDevice( i );
                m_aViews[i].m_pMSFlareScene->Stop( m_fTime );
                m_aViews[i].m_pMSFlareToFlare->Stop( m_fTime );
            }
            g_eState = SS_FLARE_IN;
            break;

        case SSMSG_FLARE_DONE:
            for( i = 0; i < m_dwNumRenderUnits; i++ )
            {
                SetDevice( i );
                s_asCWParms[2].pSrc0 = m_aViews[i].m_pFlareScene;
                s_asCWParms[2].pSrc1 = NULL;
                m_aViews[i].m_pFlareToLogo->Setup( &s_asCWParms[2] );
                m_aViews[i].m_pFlareToLogo->Go( m_fTime );
            }
            g_eState = SS_FLARE_TO_LOGO;
            break;

        case SSMSG_FLARE_TO_LOGO_DONE:
            for( i = 0; i < m_dwNumRenderUnits; i++ )
            {
                SetDevice( i );
                m_aViews[i].m_pFlareToLogo->Stop( m_fTime );
                m_aViews[i].m_pFlareScene->Stop( m_fTime );
                m_aViews[i].m_pFlareScene->Reset();
                m_aViews[i].m_pLogoScene->Go( m_fTime );
            }
            g_eState = SS_LOGO;
            break;

        default:
            break;
    }

    g_eMsg = SSMSG_NONE;

    if( g_eState == SS_START )
    {
        for( i = 0; i < m_dwNumRenderUnits; i++ )
        {
            SetDevice( i );
            m_aViews[i].m_pLogoScene->Go( m_fTime );
        }
        g_eState = SS_LOGO;
    }

    switch( g_eState )
    {
        case SS_LOGO:
            for( i = 0; i < m_dwNumRenderUnits; i++ )
            {
                SetDevice( i );
                m_aViews[i].m_pLogoScene->OnFrameMove( m_fTime, m_fElapsedTime, g_mIdent );
            }
            break;

        case SS_LOGO_TO_MSFLARE:
            for( i = 0; i < m_dwNumRenderUnits; i++ )
            {
                SetDevice( i );
                m_aViews[i].m_pLogoToMSFlare->OnFrameMove( m_fTime, m_fElapsedTime, g_mIdent );
            }
            break;

        case SS_MSFLARE:
            for( i = 0; i < m_dwNumRenderUnits; i++ )
            {
                SetDevice( i );
                m_aViews[i].m_pMSFlareScene->OnFrameMove( m_fTime, m_fElapsedTime, g_mIdent );
            }
            break;

        case SS_MSFLARE_TO_FLARE:
            for( i = 0; i < m_dwNumRenderUnits; i++ )
            {
                SetDevice( i );
                m_aViews[i].m_pMSFlareToFlare->OnFrameMove( m_fTime, m_fElapsedTime, g_mIdent );
            }
            break;

        case SS_FLARE_IN:
        case SS_FLARE:
        case SS_FLARE_OUT:
            for( i = 0; i < m_dwNumRenderUnits; i++ )
            {
                SetDevice( i );
                m_aViews[i].m_pFlareScene->OnFrameMove( m_fTime, m_fElapsedTime, g_mIdent );
            }
            break;

        case SS_FLARE_TO_LOGO:
            for( i = 0; i < m_dwNumRenderUnits; i++ )
            {
                SetDevice( i );
                m_aViews[i].m_pFlareToLogo->OnFrameMove( m_fTime, m_fElapsedTime, g_mIdent );
            }
            break;

        default:
            break;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene of the current state on the current device.
//-----------------------------------------------------------------------------
HRESULT CSSBranded::Render()
{
    HRESULT hr;

    if( !m_bDeviceObjectsRestored )
    {
        // just black
        if( SUCCEEDED( g_pView->m_pd3dDevice->BeginScene() ) )
        {
            g_pView->m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1.0f, 0 );
            g_pView->m_pd3dDevice->EndScene();
        }
        return S_OK;
    }

    if( FAILED( hr = g_pViewStack->LoadIdentity() ) )
        return hr;

    if( FAILED( hr = g_pProjStack->LoadIdentity() ) )
        return hr;

    if( SUCCEEDED( g_pView->m_pd3dDevice->BeginScene() ) )
    {
        switch( g_eState )
        {
            case SS_LOGO:
                g_pView->m_pLogoScene->OnFrameRender( m_fTime, m_fElapsedTime );
                break;

            case SS_LOGO_TO_MSFLARE:
                g_pView->m_pLogoToMSFlare->OnFrameRender( m_fTime, m_fElapsedTime );
                break;

            case SS_MSFLARE:
                g_pView->m_pMSFlareScene->OnFrameRender( m_fTime, m_fElapsedTime );
                break;

            case SS_MSFLARE_TO_FLARE:
                g_pView->m_pMSFlareToFlare->OnFrameRender( m_fTime, m_fElapsedTime );
                break;

            case SS_FLARE_IN:
            case SS_FLARE:
            case SS_FLARE_OUT:
                g_pView->m_pFlareScene->OnFrameRender( m_fTime, m_fElapsedTime );
                break;

            case SS_FLARE_TO_LOGO:
                g_pView->m_pFlareToLogo->OnFrameRender( m_fTime, m_fElapsedTime );
                break;

            default:
                break;
        }

        hr = g_pView->m_pd3dDevice->EndScene();
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: FinalCleanup()
//-----------------------------------------------------------------------------
HRESULT CSSBranded::FinalCleanup()
{
    for( UINT i = 0; i < SSBRANDED_VIEWS; i++ )
    {
        SetDevice( i );
        InvalidateDeviceObjects();
        DeleteDeviceObjects();
    }

    g_eState = SS_DONE;

    if( g_pProjStack != NULL )
    {
        g_pProjStack->Release();
        g_pProjStack = NULL;
    }

    if( g_pViewStack != NULL )
    {
        g_pViewStack->Release();
        g_pViewStack = NULL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetDevice()
// Desc: Makes the view of a device the current one.
//-----------------------------------------------------------------------------
VOID CSSBranded::SetDevice( UINT iDevice )
{
    g_pView = &m_aViews[iDevice];
    g_pView->m_pRenderUnit = &m_RenderUnits[iDevice];
    g_pView->m_pd3dDevice = m_RenderUnits[iDevice].pd3dDevice;
    memcpy( &g_pView->m_d3dsdBackBuffer, &m_d3dsdBackBuffer, sizeof(D3DSURFACE_DESC) );
}




//-----------------------------------------------------------------------------
// Name: InitDeviceObjects()
//-----------------------------------------------------------------------------
HRESULT CSSBranded::InitDeviceObjects()
{
    HRESULT hr;

    if( !g_pView->m_bInit && FAILED( hr = g_pView->InitObjects() ) )
        return hr;

    return g_pView->InitDeviceObjects();
}




//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
//-----------------------------------------------------------------------------
HRESULT CSSBranded::RestoreDeviceObjects()
{
    HRESULT hr;

    if( SUCCEEDED( hr = g_pView->RestoreDeviceObjects() ) )
        m_bDeviceObjectsRestored = true;

    return hr;
}




//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
//-----------------------------------------------------------------------------
HRESULT CSSBranded::InvalidateDeviceObjects()
{
    HRESULT hr = g_pView->InvalidateDeviceObjects();

    m_bDeviceObjectsRestored = false;

    return hr;
}




//-----------------------------------------------------------------------------
// Name: DeleteDeviceObjects()
//-----------------------------------------------------------------------------
HRESULT CSSBranded::DeleteDeviceObjects()
{
    HRESULT hr = g_pView->DeleteDeviceObjects();

    m_bDeviceObjectsRestored = false;

    return hr;
}




//-----------------------------------------------------------------------------
// Name: ConfirmDevice()
// Desc: The effects need pixel shader 2.0.
//-----------------------------------------------------------------------------
HRESULT CSSBranded::ConfirmDevice( D3DCAPS9* pCaps, DWORD /*dwBehavior*/, D3DFORMAT /*fmtBackBuffer*/ )
{
    if( D3DSHADER_VERSION_MAJOR( pCaps->PixelShaderVersion ) < 2 )
        return HRESULT_FROM_WIN32( ERROR_NOT_SUPPORTED );

    return S_OK;
}
