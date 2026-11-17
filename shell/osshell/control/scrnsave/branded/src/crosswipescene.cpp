//-----------------------------------------------------------------------------
// File: crosswipescene.cpp
//
// Desc: CCrossWipeScene - see crosswipescene.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\crosswipescene.obj
//-----------------------------------------------------------------------------
#include "crosswipescene.h"
#include "ssview.h"




//-----------------------------------------------------------------------------
// Name: CCrossWipeScene()
//-----------------------------------------------------------------------------
CCrossWipeScene::CCrossWipeScene()
{
    m_pWipeMap = NULL;
    m_pSrc0Texture = NULL;
    m_cSrc0Color = D3DXCOLOR( 0.0f, 0.0f, 0.0f, 1.0f );
    m_pSrc1Texture = NULL;
    m_cSrc1Color = D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f );

    m_CWParms.idWipeMap = RESID_NONE;
    m_CWParms.pSrc0 = NULL;
    m_CWParms.pSrc1 = NULL;

    m_WipeParms.ppSrc0 = NULL;
    m_WipeParms.ppSrc1 = NULL;
    m_WipeParms.ppWipeMap = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CCrossWipeScene()
//-----------------------------------------------------------------------------
CCrossWipeScene::~CCrossWipeScene()
{
    FinalCleanup();
}




//-----------------------------------------------------------------------------
// Name: FinalCleanup()
//-----------------------------------------------------------------------------
void CCrossWipeScene::FinalCleanup()
{
    OnLostDevice();
    OnDestroyDevice();
}




//-----------------------------------------------------------------------------
// Name: Init()
//-----------------------------------------------------------------------------
HRESULT CCrossWipeScene::Init()
{
    return m_Wipe.Init();
}




//-----------------------------------------------------------------------------
// Name: Go()
//-----------------------------------------------------------------------------
void CCrossWipeScene::Go( FLOAT fTime )
{
    CScene::Go( fTime );

    memcpy( &m_Wipe.m_WipeParms, &m_WipeParms, sizeof(WIPEPARMS) );
    m_Wipe.m_eState = CAnimation::STATE_LOADED;
    m_Wipe.Setup();
    m_Wipe.Go( fTime );
}




//-----------------------------------------------------------------------------
// Name: Setup()
// Desc: Takes the scenes to wipe between; Go() starts the wipe.
//-----------------------------------------------------------------------------
void CCrossWipeScene::Setup( CROSSWIPEPARMS* pParms )
{
    memcpy( &m_CWParms, pParms, sizeof(CROSSWIPEPARMS) );

    CreateResources();

    m_WipeParms.fDuration = m_CWParms.fDuration;
    m_WipeParms.fWidth = m_CWParms.fWidth;
    m_WipeParms.ppSrc0 = ( m_CWParms.pSrc0 != NULL && m_pSrc0Texture != NULL ) ? &m_pSrc0Texture : NULL;
    m_WipeParms.ppSrc1 = ( m_CWParms.pSrc1 != NULL && m_pSrc1Texture != NULL ) ? &m_pSrc1Texture : NULL;
    m_WipeParms.cColor0 = m_CWParms.cColor0;
    m_WipeParms.cColor1 = m_CWParms.cColor1;
    m_WipeParms.ppWipeMap = ( m_pWipeMap != NULL ) ? &m_pWipeMap : NULL;
}




//-----------------------------------------------------------------------------
// Name: CreateResources()
// Desc: (Re)creates the render targets for the two scenes and loads the
//       wipe map.
//-----------------------------------------------------------------------------
void CCrossWipeScene::CreateResources()
{
    if( m_pSrc0Texture != NULL )
    {
        m_pSrc0Texture->Release();
        m_pSrc0Texture = NULL;
    }

    if( m_pSrc1Texture != NULL )
    {
        m_pSrc1Texture->Release();
        m_pSrc1Texture = NULL;
    }

    if( m_pWipeMap != NULL )
    {
        m_pWipeMap->Release();
        m_pWipeMap = NULL;
    }

    if( m_CWParms.pSrc0 == NULL )
    {
        m_WipeParms.ppSrc0 = NULL;
    }
    else
    {
        g_pView->m_pd3dDevice->CreateTexture( g_pView->m_d3dsdBackBuffer.Width, g_pView->m_d3dsdBackBuffer.Height,
                                              1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                              &m_pSrc0Texture, NULL );
        m_WipeParms.ppSrc0 = &m_pSrc0Texture;
    }

    if( m_CWParms.pSrc1 == NULL )
    {
        m_WipeParms.ppSrc1 = NULL;
    }
    else
    {
        g_pView->m_pd3dDevice->CreateTexture( g_pView->m_d3dsdBackBuffer.Width, g_pView->m_d3dsdBackBuffer.Height,
                                              1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                              &m_pSrc1Texture, NULL );
        m_WipeParms.ppSrc1 = &m_pSrc1Texture;
    }

    if( m_CWParms.idWipeMap == RESID_NONE )
    {
        m_WipeParms.ppWipeMap = NULL;
    }
    else
    {
        MSLoadTextureFromResource( g_pView->m_pd3dDevice, MAKEINTRESOURCEW( (WORD)m_CWParms.idWipeMap ),
                                   RT_RCDATA, &m_pWipeMap );
        m_WipeParms.ppWipeMap = &m_pWipeMap;
    }
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
//-----------------------------------------------------------------------------
void CCrossWipeScene::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
{
    CScene::OnFrameMove( fTime, fElapsedTime, mParent );

    if( m_Wipe.GetState() == CAnimation::STATE_DONE )
        g_eMsg = m_CWParms.eMsg;

    if( m_CWParms.pSrc0 != NULL )
        m_CWParms.pSrc0->OnFrameMove( fTime, fElapsedTime, g_mIdent );

    if( m_CWParms.pSrc1 != NULL )
        m_CWParms.pSrc1->OnFrameMove( fTime, fElapsedTime, g_mIdent );

    m_Wipe.OnFrameMove( fTime, fElapsedTime, g_mIdent );
}




//-----------------------------------------------------------------------------
// Name: OnFrameRender()
// Desc: Draws both scenes into their render targets, then the wipe between
//       them into the back buffer.
//-----------------------------------------------------------------------------
void CCrossWipeScene::OnFrameRender( FLOAT fTime, FLOAT fElapsedTime )
{
    IDirect3DSurface9* pBackBuffer = NULL;
    IDirect3DSurface9* pSurface = NULL;

    g_pView->m_pd3dDevice->GetRenderTarget( 0, &pBackBuffer );

    if( m_pSrc0Texture != NULL )
    {
        m_pSrc0Texture->GetSurfaceLevel( 0, &pSurface );
        g_pView->m_pd3dDevice->SetRenderTarget( 0, pSurface );
        m_CWParms.pSrc0->OnFrameRender( fTime, fElapsedTime );

        if( pSurface != NULL )
        {
            pSurface->Release();
            pSurface = NULL;
        }
    }

    if( m_pSrc1Texture != NULL )
    {
        m_pSrc1Texture->GetSurfaceLevel( 0, &pSurface );
        g_pView->m_pd3dDevice->SetRenderTarget( 0, pSurface );
        m_CWParms.pSrc1->OnFrameRender( fTime, fElapsedTime );

        if( pSurface != NULL )
        {
            pSurface->Release();
            pSurface = NULL;
        }
    }

    g_pView->m_pd3dDevice->SetRenderTarget( 0, pBackBuffer );
    g_pView->m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1.0f, 0 );

    m_Wipe.OnFrameRender( fTime, fElapsedTime );

    if( pBackBuffer != NULL )
        pBackBuffer->Release();
}




//-----------------------------------------------------------------------------
// Name: OnCreateDevice()
//-----------------------------------------------------------------------------
HRESULT CCrossWipeScene::OnCreateDevice()
{
    HRESULT hr;

    if( FAILED( hr = m_Wipe.OnCreateDevice() ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnResetDevice()
//-----------------------------------------------------------------------------
HRESULT CCrossWipeScene::OnResetDevice()
{
    HRESULT hr;

    CScene::OnResetDevice();

    CreateResources();

    if( FAILED( hr = m_Wipe.OnResetDevice() ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnLostDevice()
//-----------------------------------------------------------------------------
void CCrossWipeScene::OnLostDevice()
{
    if( m_pSrc0Texture != NULL )
    {
        m_pSrc0Texture->Release();
        m_pSrc0Texture = NULL;
    }

    if( m_pSrc1Texture != NULL )
    {
        m_pSrc1Texture->Release();
        m_pSrc1Texture = NULL;
    }

    if( m_pWipeMap != NULL )
    {
        m_pWipeMap->Release();
        m_pWipeMap = NULL;
    }

    m_Wipe.OnLostDevice();
}




//-----------------------------------------------------------------------------
// Name: OnDestroyDevice()
//-----------------------------------------------------------------------------
void CCrossWipeScene::OnDestroyDevice()
{
    m_Wipe.OnDestroyDevice();
}
