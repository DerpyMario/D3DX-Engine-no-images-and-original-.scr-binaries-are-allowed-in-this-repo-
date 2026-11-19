//-----------------------------------------------------------------------------
// File: state.cpp
//
// Desc: STATE: the pipes drawn on one device - the textures, the light, the
//       grid of nodes and the "draw threads" (one per pipe being drawn).
//
//       Reconstructed from state.obj:
//           d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\state.obj
//-----------------------------------------------------------------------------
#include "stdafx.h"

// Chase mode: the other pipes follow the lead pipe
#define DRAW_NORMAL     0
#define DRAW_CHASE      1




//-----------------------------------------------------------------------------
// Name: DRAW_THREAD()
// Desc:
//-----------------------------------------------------------------------------
DRAW_THREAD::DRAW_THREAD()
{
    m_pd3dDevice = NULL;
    m_pPipe = NULL;
    m_pTextureInfo = NULL;
}

DRAW_THREAD::~DRAW_THREAD()
{
}




//-----------------------------------------------------------------------------
// Name: StartPipe()
// Desc: Returns FALSE if the pipe found no empty node to start from
//-----------------------------------------------------------------------------
BOOL DRAW_THREAD::StartPipe()
{
    m_pPipe->Start();

    return m_pPipe->m_status != PIPE_OUT_OF_NODES;
}




//-----------------------------------------------------------------------------
// Name: KillPipe()
// Desc:
//-----------------------------------------------------------------------------
void DRAW_THREAD::KillPipe()
{
    if( m_pPipe )
    {
        delete m_pPipe;
        m_pPipe = NULL;
    }
}




//-----------------------------------------------------------------------------
// Name: STATE()
// Desc:
//-----------------------------------------------------------------------------
STATE::STATE( CONFIG* pConfig )
{
    ZeroMemory( m_texInfo, sizeof(m_texInfo) );

    m_radius = 0.0f;
    m_fLastTime = 0.0f;
    m_pConfig = pConfig;
    m_resetStatus = 0;
    m_pClearVB = NULL;
    m_pNState = NULL;
    m_pd3dDevice = NULL;
    m_pWorldMatrixStack = NULL;
    m_pLeadPipe = NULL;
    m_nodes = NULL;
    m_pFState = NULL;
    m_maxDrawThreads = 0;
    m_nTextures = 0;
    m_bUseTexture = FALSE;
    m_nSlices = 0;
    m_drawScheme = TYPE_NORMAL;
    m_maxPipesPerFrame = 0;
    m_nPipesDrawn = 0;
    m_nDrawThreads = 0;
    m_drawMode = DRAW_NORMAL;
}




//-----------------------------------------------------------------------------
// Name: ~STATE()
// Desc:
//-----------------------------------------------------------------------------
STATE::~STATE()
{
    SAFE_DELETE( m_pNState );
    SAFE_DELETE( m_pFState );
    SAFE_DELETE( m_nodes );

    // Delete any pipes that are still around
    for( int i = 0; i < m_maxDrawThreads; i++ )
        m_drawThreads[i].KillPipe();
}




//-----------------------------------------------------------------------------
// Name: InitDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT STATE::InitDeviceObjects( IDirect3DDevice8* pd3dDevice )
{
    m_pd3dDevice = pd3dDevice;

    if( m_view.SetWinSize( g_pMyPipesScreensaver->m_d3dsdBackBuffer.Width,
                           g_pMyPipesScreensaver->m_d3dsdBackBuffer.Height ) )
    {
        m_resetStatus |= RESET_RESIZE_BIT;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc: Textures, render states, light, materials; the pipe state of the
//       chosen kind of pipes
//-----------------------------------------------------------------------------
HRESULT STATE::RestoreDeviceObjects()
{
    int i;
    int anDefaultTextureResource[1] = { IDB_DEFTEX };

    m_bUseTexture = FALSE;
    if( m_pConfig->bTextured )
    {
        if( SUCCEEDED( LoadTextureFiles( 1, m_pConfig->strTextureName, anDefaultTextureResource ) ) )
            m_bUseTexture = TRUE;
    }

    // (m_maxDrawThreads is not set yet)
    for( i = 0; i < m_maxDrawThreads; i++ )
        m_drawThreads[i].m_pd3dDevice = m_pd3dDevice;

    D3DXCreateMatrixStack( 0, &m_pWorldMatrixStack );

    m_view.SetProjMatrix( m_pd3dDevice );

    // Texture stages: modulate the texture with the lighting if possible
    D3DCAPS8 caps;
    ZeroMemory( &caps, sizeof(caps) );
    m_pd3dDevice->GetDeviceCaps( &caps );

    if( caps.TextureOpCaps & D3DTEXOPCAPS_MODULATE )
    {
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    }
    else if( caps.TextureOpCaps & D3DTEXOPCAPS_SELECTARG1 )
    {
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1,
            m_bUseTexture ? D3DTA_TEXTURE : D3DTA_DIFFUSE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    }

    if( caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFLINEAR )
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    else if( caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFPOINT )
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_POINT );

    if( caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR )
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    else if( caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFPOINT )
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_POINT );

    if( caps.TextureAddressCaps & D3DPTADDRESSCAPS_WRAP )
    {
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
        m_pd3dDevice->SetRenderState( D3DRS_WRAP0, D3DWRAP_U | D3DWRAP_V );
    }
    else if( caps.TextureAddressCaps & D3DPTADDRESSCAPS_CLAMP )
    {
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    }

    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 0x00 );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
    m_pd3dDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_NORMALIZENORMALS, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE,
        (caps.PrimitiveMiscCaps & D3DPMISCCAPS_CULLCW) ? D3DCULL_CW : D3DCULL_NONE );

    // Light
    ZeroMemory( &m_light, sizeof(m_light) );
    m_light.Diffuse.r = 1.0f;
    m_light.Diffuse.g = 1.0f;
    m_light.Diffuse.b = 1.0f;
    m_light.Diffuse.a = 1.0f;
    m_light.Type = D3DLIGHT_DIRECTIONAL;
    if( m_bUseTexture )
    {
        m_light.Specular.r = 0.0f;
        m_light.Specular.g = 0.0f;
        m_light.Specular.b = 0.0f;
    }
    else
    {
        m_light.Specular.r = 0.6f;
        m_light.Specular.g = 0.6f;
        m_light.Specular.b = 0.6f;
    }
    m_light.Specular.a = 1.0f;
    m_light.Position.x = 0.0f;
    m_light.Ambient.a = 1.1f;
    m_light.Position.y = -50.0f;
    m_light.Position.z = -150.0f;
    m_light.Ambient.r = 0.1f;
    m_light.Ambient.g = 0.1f;
    m_light.Ambient.b = 0.1f;
    D3DXVECTOR3 vDir( 0.0f, -50.0f, -150.0f );
    D3DXVec3Normalize( (D3DXVECTOR3*)&m_light.Direction, &vDir );
    m_light.Range = 1000.0f;
    m_pd3dDevice->SetLight( 0, &m_light );
    m_pd3dDevice->LightEnable( 0, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENT, m_bUseTexture ? 0xff2f2f2f : 0xffffffff );

    InitMaterials();

    // Pipe radius and the number of slices around it: 8, 12, 20, 28 or 36
    // for the tessellation factors 0 .. 4
    m_radius = 1.0f;
    m_nSlices = (2 - (int)( (FLOAT)(m_pConfig->dwTesselFactor * 4) / -2.0001f )) * 4;

    // The grid of nodes
    m_nodes = new NODE_ARRAY;

    // Multiple pipes at a time
    m_maxDrawThreads = m_pConfig->bMultiPipes ? MAX_DRAW_THREADS : 1;
    m_nDrawThreads = 0;
    m_nPipesDrawn = 0;

    // The quad that fades the screen out between two rounds of pipes
    SAFE_RELEASE( m_pClearVB );
    m_pd3dDevice->CreateVertexBuffer( 4 * sizeof(FADEVERTEX), D3DUSAGE_WRITEONLY,
                                      D3DFVF_FADEVERTEX, D3DPOOL_MANAGED, &m_pClearVB );
    FADEVERTEX* pVertices;
    m_pClearVB->Lock( 0, 0, (BYTE**)&pVertices, 0 );
    for( i = 0; i < 4; i++ )
    {
        pVertices[i].x = 0.0f;
        pVertices[i].y = 0.0f;
        pVertices[i].z = 0.95f;
        pVertices[i].rhw = 1.0f;
        pVertices[i].color = 0x20000000;
    }
    pVertices[0].y = (FLOAT)m_view.m_winSize.cy;
    pVertices[1].y = (FLOAT)m_view.m_winSize.cy;
    pVertices[1].x = (FLOAT)m_view.m_winSize.cx;
    pVertices[3].x = (FLOAT)m_view.m_winSize.cx;
    m_pClearVB->Unlock();

    // The kind of pipes
    if( !m_pConfig->bFlexMode )
    {
        m_drawScheme = TYPE_NORMAL;
        m_pNState = new NORMAL_STATE( this );
        m_pFState = NULL;
    }
    else
    {
        m_drawScheme = TYPE_FLEX_REGULAR;
        m_pFState = new FLEX_STATE( this );
        m_pNState = NULL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT STATE::InvalidateDeviceObjects()
{
    SAFE_RELEASE( m_pClearVB );

    for( int i = 0; i < m_nTextures; i++ )
        SAFE_RELEASE( m_texInfo[i].pTexture );

    SAFE_RELEASE( m_pWorldMatrixStack );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DeleteDeviceObjects(), FrameMove()
// Desc: (nothing to do)
//-----------------------------------------------------------------------------
HRESULT STATE::DeleteDeviceObjects()
{
    return S_OK;
}

HRESULT STATE::FrameMove( FLOAT fElapsedTime )
{
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: LoadTextureFiles()
// Desc: Loads the textures: the files the user chose, or else the textures
//       in the resources.
//-----------------------------------------------------------------------------
HRESULT STATE::LoadTextureFiles( int nTextures, TCHAR strTextureFiles[][MAX_TEXTURES],
                                 int* anDefaultTextureResource )
{
    HRESULT hr;
    WIN32_FIND_DATA findData;

    m_nTextures = 0;

    for( int i = 0; i < nTextures; i++ )
    {
        SAFE_RELEASE( m_texInfo[i].pTexture );

        if( !m_pConfig->bDefaultTexture &&
            FindFirstFile( strTextureFiles[i], &findData ) != INVALID_HANDLE_VALUE &&
            FAILED( hr = D3DXCreateTextureFromFileEx( m_pd3dDevice, strTextureFiles[i],
                D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                D3DX_FILTER_TRIANGLE | D3DX_FILTER_MIRROR, D3DX_FILTER_TRIANGLE | D3DX_FILTER_MIRROR,
                0, NULL, NULL, &m_texInfo[i].pTexture ) ) )
        {
            SAFE_RELEASE( m_texInfo[i].pTexture );
        }

        if( m_texInfo[i].pTexture == NULL )
        {
            if( FAILED( hr = D3DXCreateTextureFromResourceEx( m_pd3dDevice, NULL,
                MAKEINTRESOURCE( anDefaultTextureResource[i] ),
                D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                D3DX_FILTER_TRIANGLE | D3DX_FILTER_MIRROR, D3DX_FILTER_TRIANGLE | D3DX_FILTER_MIRROR,
                0, NULL, NULL, &m_texInfo[i].pTexture ) ) )
            {
                SAFE_RELEASE( m_texInfo[i].pTexture );
            }

            if( m_texInfo[i].pTexture == NULL )
                return hr;
        }

        D3DSURFACE_DESC d3dDesc;
        ZeroMemory( &d3dDesc, sizeof(d3dDesc) );
        m_texInfo[i].pTexture->GetLevelDesc( 0, &d3dDesc );
        m_texInfo[i].width = d3dDesc.Width;
        m_texInfo[i].height = d3dDesc.Height;
    }

    m_nTextures = nTextures;

    CalcTexRepFactors();

    return hr;
}




//-----------------------------------------------------------------------------
// Name: CalcTexRepFactors()
// Desc: How often each texture repeats on a pipe segment: in proportion to
//       how many times it would fit across the window, eighth of it.
//-----------------------------------------------------------------------------
void STATE::CalcTexRepFactors()
{
    SIZE winSize = m_view.m_winSize;
    int i;

    for( i = 0; i < m_nTextures; i++ )
    {
        FLOAT f;

        m_texRep[i].x = m_texRep[i].y = 1;

        f = (FLOAT)(winSize.cx / m_texInfo[i].width) * 0.125f;
        if( f >= 1.0f )
            m_texRep[i].x = (int)(f + 0.5f);

        f = (FLOAT)(winSize.cy / m_texInfo[i].height) * 0.125f;
        if( f >= 1.0f )
            m_texRep[i].y = (int)(f + 0.5f);
    }

    // The objects of the normal pipes are shared by all textures: use the
    // smallest repetition for all of them
    if( m_pNState && m_nTextures > 1 )
    {
        for( i = 1; i < m_nTextures; i++ )
        {
            if( m_texRep[i].x < m_texRep[0].x )
                m_texRep[0].x = m_texRep[i].x;
            if( m_texRep[i].y < m_texRep[0].y )
                m_texRep[0].y = m_texRep[i].y;
        }
    }
}




//-----------------------------------------------------------------------------
// Name: Repaint()
// Desc: Start over (the window was uncovered)
//-----------------------------------------------------------------------------
void STATE::Repaint()
{
    m_resetStatus |= RESET_REPAINT_BIT;
}




//-----------------------------------------------------------------------------
// Name: ResetView()
// Desc: Sizes the grid of nodes for the window
//-----------------------------------------------------------------------------
void STATE::ResetView()
{
    IPOINT3D numNodes;

    m_view.CalcNodeArraySize( &numNodes );
    m_nodes->Resize( &numNodes );
}




//-----------------------------------------------------------------------------
// Name: CalcMaxPipesPerFrame()
// Desc: How many pipes are drawn before the screen is cleared
//-----------------------------------------------------------------------------
int STATE::CalcMaxPipesPerFrame()
{
    int nCount = 0;
    int fCount = 0;

    if( m_pFState )
        fCount = m_pFState->GetMaxPipesPerFrame();

    if( m_pNState )
        nCount = m_bUseTexture ? 3 : 5;

    return max( fCount, nCount );
}




//-----------------------------------------------------------------------------
// Name: PickRandomTexture()
// Desc: Picks one of the textures, each one once before any is used twice.
//-----------------------------------------------------------------------------
int STATE::PickRandomTexture( int iThread, int nTextures )
{
    static int pickSet[MAX_TEXTURES] = {0};
    static int nPicked = 0;
    int i;

    if( nTextures == 0 )
        return 0;

    // Start a new set with the first thread
    if( iThread == 0 )
        nPicked = nTextures;

    if( ++nPicked > nTextures )
    {
        for( i = 0; i < nTextures; i++ )
            pickSet[i] = 0;
        nPicked = 1;
    }

    i = CPipesScreensaver::iRand( nTextures );
    while( pickSet[i] )
    {
        if( ++i >= nTextures )
            i = 0;
    }
    pickSet[i] = 1;

    return i;
}




//-----------------------------------------------------------------------------
// Name: Clear()
// Desc: Clears the screen for a new round of pipes.  After a normal round
//       the old pipes are faded out over 30 steps of 16 ms first; returns
//       TRUE once the screen is clear.
//-----------------------------------------------------------------------------
BOOL STATE::Clear()
{
    if( m_resetStatus & RESET_NORMAL_BIT )
    {
        static FLOAT fLastStepTime = DXUtil_Timer( TIMER_GETAPPTIME );
        static int nSteps = 0;

        if( nSteps == 0 )
            nSteps = 30;

        FLOAT fTime = DXUtil_Timer( TIMER_GETAPPTIME );
        if( fTime - fLastStepTime > 0.016f )
        {
            nSteps--;
            fLastStepTime = fTime;

            if( nSteps == 0 )
            {
                m_pd3dDevice->SetTexture( 0, NULL );
                m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
                goto clear;
            }

            // Darken the screen a little
            m_pd3dDevice->SetTexture( 0, NULL );
            m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
            m_pd3dDevice->SetVertexShader( D3DFVF_FADEVERTEX );
            m_pd3dDevice->SetStreamSource( 0, m_pClearVB, sizeof(FADEVERTEX) );
            m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
        }
        return FALSE;
    }

clear:
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0L );
    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: ChooseNewLeadPipe()
// Desc: The lead pipe of a chase died: another one takes over
//-----------------------------------------------------------------------------
void STATE::ChooseNewLeadPipe()
{
    int iThread = CPipesScreensaver::iRand( m_nDrawThreads );

    m_pLeadPipe = m_drawThreads[iThread].m_pPipe;
    m_pLeadPipe->SetChooseStartPosMethod( CHOOSE_STARTPOS_FURTHEST );
    m_pLeadPipe->SetChooseDirectionMethod( CHOOSE_DIR_RANDOM_WEIGHTED );
}




//-----------------------------------------------------------------------------
// Name: FrameReset()
// Desc: Starts a new round of pipes.  Returns FALSE while the screen is
//       still fading out.
//-----------------------------------------------------------------------------
BOOL STATE::FrameReset()
{
    int i;

    for( i = 0; i < m_nDrawThreads; i++ )
        m_drawThreads[i].KillPipe();
    m_nDrawThreads = 0;

    if( !Clear() )
        return FALSE;

    if( m_resetStatus & RESET_RESIZE_BIT )
        ResetView();

    m_nodes->Reset();

    if( m_pNState )
        m_pNState->Reset();

    if( m_pFState )
    {
        m_pFState->Reset();

        // (a random tilt of the scene in the OpenGL version)
        FLOAT xRot = CPipesScreensaver::fRand( -5.0f, 5.0f );
        FLOAT zRot = CPipesScreensaver::fRand( -5.0f, 5.0f );
        (void)xRot;
        (void)zRot;
    }

    m_maxPipesPerFrame = CalcMaxPipesPerFrame();

    if( m_maxDrawThreads > 1 )
    {
        // A random number of pipes at a time
        m_maxPipesPerFrame = (int)( m_maxPipesPerFrame * 1.5 );
        m_nDrawThreads = min( m_maxPipesPerFrame,
                              CPipesScreensaver::iRand2( 2, m_maxDrawThreads ) );

        // ... chasing each other now and then
        if( ( m_pNState || ( m_pFState && m_pFState->OKToUseChase() ) ) &&
            CPipesScreensaver::iRand( 5 ) == 0 )
        {
            m_drawMode = DRAW_CHASE;
        }
    }
    else
    {
        m_nDrawThreads = 1;
    }

    m_nPipesDrawn = 0;

    PIPE* pNewPipe = NULL;
    for( i = 0; i < m_nDrawThreads; i++ )
    {
        DRAW_THREAD* pThread = &m_drawThreads[i];

        // Every pipe starts from the scene's rotation
        D3DXVECTOR3 yAxis( 0.0f, 1.0f, 0.0f );
        m_pWorldMatrixStack->LoadIdentity();
        m_pWorldMatrixStack->RotateAxis( &yAxis, m_view.m_yRot );   // (degrees, as in OpenGL)

        if( m_drawScheme == TYPE_NORMAL )
            pNewPipe = new NORMAL_PIPE( this );
        else if( m_drawScheme == TYPE_FLEX_REGULAR )
            pNewPipe = m_pFState->NewPipe( this );

        if( pNewPipe == NULL )
            return FALSE;

        pThread->m_pPipe = pNewPipe;

        if( m_drawMode == DRAW_CHASE )
        {
            if( i == 0 )
                m_pLeadPipe = pNewPipe;
            pNewPipe->SetChooseDirectionMethod( i != 0 ? CHOOSE_DIR_CHASE : CHOOSE_DIR_RANDOM_WEIGHTED );
        }

        if( m_bUseTexture )
        {
            int iTex = PickRandomTexture( i, m_nTextures );
            pThread->m_pTextureInfo = &m_texInfo[iTex];

            if( m_pFState )
                ((FLEX_PIPE*)pNewPipe)->SetTexParams( &m_texInfo[iTex], &m_texRep[iTex] );
        }

        pThread->StartPipe();

        // The lead pipe starts far away from the others
        if( i == 0 && m_drawMode == DRAW_CHASE )
            pNewPipe->SetChooseStartPosMethod( CHOOSE_STARTPOS_FURTHEST );

        m_nPipesDrawn++;
    }

    // A new round: turn the scene a little
    if( m_resetStatus & RESET_NORMAL_BIT )
        m_view.IncrementSceneRotation();

    m_resetStatus = 0;

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: CompactThreadList()
// Desc: Moves the threads with a pipe to the front of the list
//-----------------------------------------------------------------------------
void STATE::CompactThreadList()
{
    if( m_nDrawThreads <= 1 )
        return;

    int iEmpty = 0;
    DRAW_THREAD* pThread = m_drawThreads;
    for( int i = 0; i < m_nDrawThreads; i++ )
    {
        if( m_drawThreads[i].m_pPipe )
        {
            if( iEmpty < i )
            {
                DRAW_THREAD tmp = *pThread;
                *pThread = m_drawThreads[i];
                m_drawThreads[i] = tmp;
            }
            iEmpty++;
            pThread++;
        }
    }
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the next segment of every pipe.  Stuck pipes are restarted
//       somewhere else until enough pipes have been drawn.
//-----------------------------------------------------------------------------
HRESULT STATE::Render()
{
    int i;
    int nKilledThreads = 0;
    BOOL bChooseNewLead = FALSE;

    if( m_resetStatus != 0 && !FrameReset() )
        return S_OK;

    for( i = 0; i < m_nDrawThreads; i++ )
    {
        DRAW_THREAD* pThread = &m_drawThreads[i];

        if( pThread->m_pPipe->IsStuck() )
        {
            m_nPipesDrawn++;
            if( m_nPipesDrawn > m_maxPipesPerFrame )
            {
                // Enough pipes: kill this one
                if( m_drawMode == DRAW_CHASE && pThread->m_pPipe == m_pLeadPipe )
                    bChooseNewLead = TRUE;

                pThread->KillPipe();
                nKilledThreads++;
            }
            else
            {
                // Start it again somewhere else
                if( !pThread->StartPipe() )
                    m_maxPipesPerFrame = m_nPipesDrawn;
            }
        }
    }

    if( nKilledThreads )
    {
        CompactThreadList();
        m_nDrawThreads -= nKilledThreads;
    }

    if( m_nDrawThreads == 0 )
    {
        // All pipes are done: start over
        m_resetStatus |= RESET_NORMAL_BIT;
        return S_OK;
    }

    if( bChooseNewLead )
        ChooseNewLeadPipe();

    for( i = 0; i < m_nDrawThreads; i++ )
        m_drawThreads[i].m_pPipe->Draw();

    return S_OK;
}
