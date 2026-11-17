//-----------------------------------------------------------------------------
// File: ssview.cpp
//
// Desc: CSSView - see ssview.h.
//
//       InitObjects(), InitDeviceObjects() and RestoreDeviceObjects() work on
//       the current view (g_pView), which is the view they are called for.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\ssview.obj
//-----------------------------------------------------------------------------
#include <new>
#include "ssview.h"




//-----------------------------------------------------------------------------
// Name: CSSView()
//-----------------------------------------------------------------------------
CSSView::CSSView()
{
    m_pd3dDevice = NULL;
    m_pRenderUnit = NULL;
    m_bInit = false;

    m_pLogoScene = NULL;
    m_pLogoToMSFlare = NULL;
    m_pMSFlareScene = NULL;
    m_pMSFlareToFlare = NULL;
    m_pFlareScene = NULL;
    m_pFlareToLogo = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CSSView()
//-----------------------------------------------------------------------------
CSSView::~CSSView()
{
    InvalidateDeviceObjects();
    DeleteDeviceObjects();

    if( m_pLogoScene != NULL )
    {
        delete m_pLogoScene;
        m_pLogoScene = NULL;
    }
    if( m_pLogoToMSFlare != NULL )
    {
        delete m_pLogoToMSFlare;
        m_pLogoToMSFlare = NULL;
    }
    if( m_pMSFlareScene != NULL )
    {
        delete m_pMSFlareScene;
        m_pMSFlareScene = NULL;
    }
    if( m_pMSFlareToFlare != NULL )
    {
        delete m_pMSFlareToFlare;
        m_pMSFlareToFlare = NULL;
    }
    if( m_pFlareScene != NULL )
    {
        delete m_pFlareScene;
        m_pFlareScene = NULL;
    }
    if( m_pFlareToLogo != NULL )
    {
        delete m_pFlareToLogo;
        m_pFlareToLogo = NULL;
    }
}




//-----------------------------------------------------------------------------
// Name: InitObjects()
// Desc: Creates the scenes and initializes all the objects.  Done once, for
//       the first device the view gets.
//-----------------------------------------------------------------------------
HRESULT CSSView::InitObjects()
{
    HRESULT hr;
    CSSView* pView = g_pView;

    pView->m_Logo.Init();
    pView->m_MSFlareBgnd.Init();
    pView->m_MSFlare.Init();
    pView->m_SolidColorWipe.Init();
    pView->m_Bgnd.Init();

    if( FAILED( hr = pView->m_Flare.Init() ) )
        goto LExit;

    pView->m_pLogoScene = new( std::nothrow ) CLogoScene;
    hr = ( pView->m_pLogoScene == NULL ) ? E_OUTOFMEMORY : pView->m_pLogoScene->Init();
    if( FAILED( hr ) )
        goto LExit;

    pView->m_pLogoToMSFlare = new( std::nothrow ) CCrossWipeScene;
    hr = ( pView->m_pLogoToMSFlare == NULL ) ? E_OUTOFMEMORY : pView->m_pLogoToMSFlare->Init();
    if( FAILED( hr ) )
        goto LExit;

    pView->m_pMSFlareScene = new( std::nothrow ) CMSFlareScene;
    hr = ( pView->m_pMSFlareScene == NULL ) ? E_OUTOFMEMORY : pView->m_pMSFlareScene->Init();
    if( FAILED( hr ) )
        goto LExit;

    pView->m_pMSFlareToFlare = new( std::nothrow ) CCrossWipeScene;
    hr = ( pView->m_pMSFlareToFlare == NULL ) ? E_OUTOFMEMORY : pView->m_pMSFlareToFlare->Init();
    if( FAILED( hr ) )
        goto LExit;

    pView->m_pFlareScene = new( std::nothrow ) CFlareScene;
    hr = ( pView->m_pFlareScene == NULL ) ? E_OUTOFMEMORY : pView->m_pFlareScene->Init();
    if( FAILED( hr ) )
        goto LExit;

    pView->m_pFlareToLogo = new( std::nothrow ) CCrossWipeScene;
    hr = ( pView->m_pFlareToLogo == NULL ) ? E_OUTOFMEMORY : pView->m_pFlareToLogo->Init();

LExit:
    pView->m_bInit = true;

    return hr;
}




//-----------------------------------------------------------------------------
// Name: InitDeviceObjects()
//-----------------------------------------------------------------------------
HRESULT CSSView::InitDeviceObjects()
{
    HRESULT hr;
    CSSView* pView = g_pView;

    if( FAILED( hr = pView->m_Logo.OnCreateDevice() ) ||
        FAILED( hr = pView->m_MSFlareBgnd.OnCreateDevice() ) ||
        FAILED( hr = pView->m_MSFlare.OnCreateDevice() ) ||
        FAILED( hr = pView->m_SolidColorWipe.OnCreateDevice() ) ||
        FAILED( hr = pView->m_Bgnd.OnCreateDevice() ) ||
        FAILED( hr = pView->m_Flare.OnCreateDevice() ) ||
        FAILED( hr = pView->m_pLogoScene->OnCreateDevice() ) ||
        FAILED( hr = pView->m_pLogoToMSFlare->OnCreateDevice() ) ||
        FAILED( hr = pView->m_pMSFlareScene->OnCreateDevice() ) ||
        FAILED( hr = pView->m_pMSFlareToFlare->OnCreateDevice() ) ||
        FAILED( hr = pView->m_pFlareScene->OnCreateDevice() ) )
    {
        return hr;
    }

    return pView->m_pFlareToLogo->OnCreateDevice();
}




//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
//-----------------------------------------------------------------------------
HRESULT CSSView::RestoreDeviceObjects()
{
    HRESULT hr;
    CSSView* pView = g_pView;

    if( FAILED( hr = pView->m_Logo.OnResetDevice() ) ||
        FAILED( hr = pView->m_MSFlareBgnd.OnResetDevice() ) ||
        FAILED( hr = pView->m_MSFlare.OnResetDevice() ) ||
        FAILED( hr = pView->m_SolidColorWipe.OnResetDevice() ) ||
        FAILED( hr = pView->m_Bgnd.OnResetDevice() ) ||
        FAILED( hr = pView->m_Flare.OnResetDevice() ) ||
        FAILED( hr = pView->m_pLogoScene->OnResetDevice() ) ||
        FAILED( hr = pView->m_pLogoToMSFlare->OnResetDevice() ) ||
        FAILED( hr = pView->m_pMSFlareScene->OnResetDevice() ) ||
        FAILED( hr = pView->m_pMSFlareToFlare->OnResetDevice() ) ||
        FAILED( hr = pView->m_pFlareScene->OnResetDevice() ) )
    {
        return hr;
    }

    return pView->m_pFlareToLogo->OnResetDevice();
}




//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
//-----------------------------------------------------------------------------
HRESULT CSSView::InvalidateDeviceObjects()
{
    m_Logo.OnLostDevice();
    m_MSFlareBgnd.OnLostDevice();
    m_MSFlare.OnLostDevice();
    m_SolidColorWipe.OnLostDevice();
    m_Bgnd.OnLostDevice();
    m_Flare.OnLostDevice();

    if( m_pLogoScene != NULL )
        m_pLogoScene->OnLostDevice();
    if( m_pLogoToMSFlare != NULL )
        m_pLogoToMSFlare->OnLostDevice();
    if( m_pMSFlareScene != NULL )
        m_pMSFlareScene->OnLostDevice();
    if( m_pMSFlareToFlare != NULL )
        m_pMSFlareToFlare->OnLostDevice();
    if( m_pFlareScene != NULL )
        m_pFlareScene->OnLostDevice();
    if( m_pFlareToLogo != NULL )
        m_pFlareToLogo->OnLostDevice();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DeleteDeviceObjects()
//-----------------------------------------------------------------------------
HRESULT CSSView::DeleteDeviceObjects()
{
    m_Logo.OnDestroyDevice();
    m_MSFlareBgnd.OnDestroyDevice();
    m_MSFlare.OnDestroyDevice();
    m_SolidColorWipe.OnDestroyDevice();
    m_Bgnd.OnDestroyDevice();
    m_Flare.OnDestroyDevice();

    if( m_pLogoScene != NULL )
        m_pLogoScene->OnDestroyDevice();
    if( m_pLogoToMSFlare != NULL )
        m_pLogoToMSFlare->OnDestroyDevice();
    if( m_pMSFlareScene != NULL )
        m_pMSFlareScene->OnDestroyDevice();
    if( m_pMSFlareToFlare != NULL )
        m_pMSFlareToFlare->OnDestroyDevice();
    if( m_pFlareScene != NULL )
        m_pFlareScene->OnDestroyDevice();
    if( m_pFlareToLogo != NULL )
        m_pFlareToLogo->OnDestroyDevice();

    return S_OK;
}
