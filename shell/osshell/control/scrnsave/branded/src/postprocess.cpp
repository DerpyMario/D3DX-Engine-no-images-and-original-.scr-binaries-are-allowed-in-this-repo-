//-----------------------------------------------------------------------------
// File: postprocess.cpp
//
// Desc: CPostProcess, CPProcType, CRenderTargetChain - see postprocess.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\postprocess.obj
//-----------------------------------------------------------------------------
#include <new>
#include "postprocess.h"
#include "ssview.h"
#include "resource.h"
#include <strsafe.h>

const D3DVERTEXELEMENT9 PPVERT::s_Decl[4] =
{
    { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0 },
    { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0 },
    { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  1 },
    D3DDECL_END()
};

DWORD CPProcType::s_adwFxFile[PP_COUNT] =
{
    IDR_FX_PP_COLORBLOOMH,
    IDR_FX_PP_COLORGBLURV,
    IDR_FX_PP_COLORBRIGHTPASS,
    IDR_FX_PP_COLORDOWNFILTER4,
    IDR_FX_PP_COLORUPFILTER,
    IDR_FX_PP_COLORCOMBINE4,
};

// The bloom: shrink the scene to 1/16, blur it horizontally and vertically
// (BloomScale: CPostProcess::SetBloomScale()), enlarge it and combine it.
PPROCSTAGE CPostProcess::s_asStages[PP_STAGES] =
{
    { { D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 0.0f ), D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 0.0f ) }, PP_COLORDOWNFILTER4 },
    { { D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 0.0f ), D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 0.0f ) }, PP_COLORDOWNFILTER4 },
    { { D3DXVECTOR4( 1.65f, 0.0f, 0.0f, 0.0f ), D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 0.0f ) }, PP_COLORBLOOMH },
    { { D3DXVECTOR4( 1.65f, 0.0f, 0.0f, 0.0f ), D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 0.0f ) }, PP_COLORGBLURV },
    { { D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 0.0f ), D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 0.0f ) }, PP_COLORUPFILTER },
    { { D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 0.0f ), D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 0.0f ) }, PP_COLORCOMBINE4 },
};




//-----------------------------------------------------------------------------
// Name: CPProcType()
//-----------------------------------------------------------------------------
CPProcType::CPProcType()
{
    m_pEffect = NULL;
    m_hTPostProcess = NULL;
    m_nRenderTarget = 0;

    ZeroMemory( m_hTexSource, sizeof(m_hTexSource) );
    ZeroMemory( m_hTexScene, sizeof(m_hTexScene) );
    ZeroMemory( m_bWrite, sizeof(m_bWrite) );
    ZeroMemory( m_ahParam, sizeof(m_ahParam) );
    ZeroMemory( m_awszParamName, sizeof(m_awszParamName) );
    ZeroMemory( m_awszParamDesc, sizeof(m_awszParamDesc) );
    ZeroMemory( m_anParamSize, sizeof(m_anParamSize) );
    ZeroMemory( m_avParamDef, sizeof(m_avParamDef) );
}




//-----------------------------------------------------------------------------
// Name: ~CPProcType()
//-----------------------------------------------------------------------------
CPProcType::~CPProcType()
{
    Cleanup();
}




//-----------------------------------------------------------------------------
// Name: Init()
// Desc: Loads the effect and reads its description.
//-----------------------------------------------------------------------------
HRESULT CPProcType::Init( DWORD dwShaderFlags, DWORD dwResourceID )
{
    HRESULT hr;

    if( FAILED( hr = D3DXCreateEffectFromResourceW( g_pView->m_pd3dDevice, NULL,
                                                    MAKEINTRESOURCEW( (WORD)dwResourceID ), NULL, NULL,
                                                    dwShaderFlags, NULL, &m_pEffect, NULL ) ) )
    {
        return hr;
    }

    // Get the PostProcess technique handle
    m_hTPostProcess = m_pEffect->GetTechniqueByName( "PostProcess" );

    // Obtain the handles to the texture objects in the effect
    m_hTexScene[0] = m_pEffect->GetParameterByName( NULL, "g_txSceneColor" );
    m_hTexSource[0] = m_pEffect->GetParameterByName( NULL, "g_txSrcColor" );

    // Find out what render targets the technique writes to.
    D3DXTECHNIQUE_DESC techdesc;
    if( FAILED( m_pEffect->GetTechniqueDesc( m_hTPostProcess, &techdesc ) ) )
        return D3DERR_INVALIDCALL;

    for( DWORD i = 0; i < techdesc.Passes; ++i )
    {
        D3DXPASS_DESC passdesc;
        if( SUCCEEDED( m_pEffect->GetPassDesc( m_pEffect->GetPass( m_hTPostProcess, i ), &passdesc ) ) )
        {
            D3DXSEMANTIC aSem[MAXD3DDECLLENGTH];
            UINT uCount;
            if( SUCCEEDED( D3DXGetShaderOutputSemantics( passdesc.pPixelShaderFunction, aSem, &uCount ) ) )
            {
                // Semantics received. Now examine the content and
                // set the write flags accordingly.
                // (Unlike the SDK sample, the flag is indexed by the
                // position of the output, not by its usage index.)
                while( uCount-- )
                {
                    if( D3DDECLUSAGE_COLOR == aSem[uCount].Usage && RT_COUNT > aSem[uCount].UsageIndex )
                        m_bWrite[uCount] = true;
                }
            }
        }
    }

    // Obtain the render target channel
    D3DXHANDLE hAnno;
    hAnno = m_pEffect->GetAnnotationByName( m_hTPostProcess, "nRenderTarget" );
    if( hAnno != NULL )
        m_pEffect->GetInt( hAnno, &m_nRenderTarget );

    // Obtain the handles to the changeable parameters, if any.
    for( int i = 0; i < NUM_PARAMS; ++i )
    {
        char szName[32];
        LPCSTR szParamName;

        StringCchPrintfA( szName, 32, "Parameter%d", i );
        hAnno = m_pEffect->GetAnnotationByName( m_hTPostProcess, szName );
        if( hAnno != NULL && SUCCEEDED( m_pEffect->GetString( hAnno, &szParamName ) ) )
        {
            m_ahParam[i] = m_pEffect->GetParameterByName( NULL, szParamName );
            MultiByteToWideChar( CP_ACP, 0, szParamName, -1, m_awszParamName[i], MAX_PATH );
        }

        // Get the parameter description
        StringCchPrintfA( szName, 32, "Parameter%dDesc", i );
        hAnno = m_pEffect->GetAnnotationByName( m_hTPostProcess, szName );
        if( hAnno != NULL && SUCCEEDED( m_pEffect->GetString( hAnno, &szParamName ) ) )
        {
            MultiByteToWideChar( CP_ACP, 0, szParamName, -1, m_awszParamDesc[i], MAX_PATH );
        }

        // Save the parameter size
        StringCchPrintfA( szName, 32, "Parameter%dSize", i );
        hAnno = m_pEffect->GetAnnotationByName( m_hTPostProcess, szName );
        if( hAnno != NULL )
            m_pEffect->GetInt( hAnno, &m_anParamSize[i] );

        // Get the parameter default
        StringCchPrintfA( szName, 32, "Parameter%dDef", i );
        hAnno = m_pEffect->GetAnnotationByName( m_hTPostProcess, szName );
        if( hAnno != NULL )
            m_pEffect->GetVector( hAnno, &m_avParamDef[i] );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnResetDevice()
// Desc: Converts the kernels of the effect from pixel space to texel space.
//       Kernels are identified by a string annotation ConvertPixelsToTexels
//       naming the parameter with the pixel offsets.
//-----------------------------------------------------------------------------
HRESULT CPProcType::OnResetDevice( DWORD dwWidth, DWORD dwHeight )
{
    if( m_pEffect == NULL )
        return S_OK;

    m_pEffect->OnResetDevice();

    D3DXHANDLE hParamToConvert;
    D3DXHANDLE hAnnotation;
    UINT uParamIndex = 0;

    // If a top-level parameter has the "ConvertPixelsToTexels" annotation,
    // do the conversion.
    while( NULL != ( hParamToConvert = m_pEffect->GetParameter( NULL, uParamIndex++ ) ) )
    {
        if( NULL != ( hAnnotation = m_pEffect->GetAnnotationByName( hParamToConvert, "ConvertPixelsToTexels" ) ) )
        {
            LPCSTR szSource;
            m_pEffect->GetString( hAnnotation, &szSource );
            D3DXHANDLE hConvertSource = m_pEffect->GetParameterByName( NULL, szSource );

            if( hConvertSource != NULL )
            {
                // Kernel source exists. Proceed.
                // Retrieve the kernel size
                D3DXPARAMETER_DESC desc;
                m_pEffect->GetParameterDesc( hConvertSource, &desc );

                // Each element has 2 floats
                DWORD cKernel = desc.Bytes / ( 2 * sizeof(float) );
                D3DXVECTOR4* pvKernel = new( std::nothrow ) D3DXVECTOR4[cKernel];
                if( pvKernel == NULL )
                    return E_OUTOFMEMORY;

                m_pEffect->GetVectorArray( hConvertSource, pvKernel, cKernel );

                // Convert
                for( DWORD i = 0; i < cKernel; ++i )
                {
                    pvKernel[i].x = pvKernel[i].x / dwWidth;
                    pvKernel[i].y = pvKernel[i].y / dwHeight;
                }

                // Copy back
                m_pEffect->SetVectorArray( hParamToConvert, pvKernel, cKernel );

                delete[] pvKernel;
            }
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CPostProcess()
//-----------------------------------------------------------------------------
CPostProcess::CPostProcess() :
    m_pEffect( NULL ),
    m_pVertDeclPP( NULL ),
    m_iReserved( 0 ),
    m_hTRender( NULL ),
    m_nPasses( 1 ),
    m_nRtUsed( 1 )
{
    ZeroMemory( (void*)m_aPostProcess, sizeof(m_aPostProcess) );
    ZeroMemory( m_pSceneSave, sizeof(m_pSceneSave) );
    ZeroMemory( (void*)m_RTChain, sizeof(m_RTChain) );
    m_pSceneSurface = NULL;
    m_cStages = PP_STAGES;
    m_fBloomScale = 1.65f;
}




//-----------------------------------------------------------------------------
// Name: ~CPostProcess()
//-----------------------------------------------------------------------------
CPostProcess::~CPostProcess()
{
    OnLostDevice();
    OnDestroyDevice();
}




//-----------------------------------------------------------------------------
// Name: OnFrameRender()
// Desc: Runs the chain over the scene (in m_pSceneSave) and adds the result
//       onto pBackBuffer.
//-----------------------------------------------------------------------------
void CPostProcess::OnFrameRender( IDirect3DSurface9* pBackBuffer )
{
    // Swap the chains
    for( int i = 0; i < RT_COUNT; ++i )
        m_RTChain[i].Flip();

    for( int rt = 1; rt < m_nRtUsed; ++rt )
        g_pView->m_pd3dDevice->SetRenderTarget( rt, NULL );

    PerformPostProcess();

    // Restore the back buffer
    g_pView->m_pd3dDevice->SetRenderTarget( 0, pBackBuffer );

    //
    // Get the final result image onto the backbuffer
    //
    UINT uWidth = g_pView->m_d3dsdBackBuffer.Width;
    UINT uHeight = g_pView->m_d3dsdBackBuffer.Height;

    PPVERT quad[4] =
    {
        { -0.5f,          -0.5f,           0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { uWidth - 0.5f,  -0.5f,           0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f },
        { -0.5f,          uHeight - 0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },
        { uWidth - 0.5f,  uHeight - 0.5f,  0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f }
    };

    IDirect3DTexture9* pPrevTarget = m_RTChain[0].GetPrevTarget();

    g_pView->m_pd3dDevice->SetVertexDeclaration( m_pVertDeclPP );
    m_pEffect->SetTechnique( m_hTRender );
    m_pEffect->SetTexture( "g_txSrc", pPrevTarget );
    m_pEffect->SetFloatArray( "g_cTargetColor", (FLOAT*)&g_cTargetColor, 3 );

    UINT cPasses;
    m_pEffect->Begin( &cPasses, 0 );
    for( UINT p = 0; p < cPasses; ++p )
    {
        m_pEffect->BeginPass( p );
        g_pView->m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, quad, sizeof(PPVERT) );
        m_pEffect->EndPass();
    }
    m_pEffect->End();
}




//-----------------------------------------------------------------------------
// Name: PerformSinglePostProcess()
// Desc: Runs one stage of the chain.
//-----------------------------------------------------------------------------
HRESULT CPostProcess::PerformSinglePostProcess( CPProcType& PP, PPROCSTAGE* pStage, IDirect3DVertexBuffer9* pVB,
                                                PPVERT* aQuad, FLOAT& fExtentX, FLOAT& fExtentY )
{
    HRESULT hr;

    //
    // The post-process effect may require that a copy of the
    // originally rendered scene be available for use, so
    // we initialize them here.
    //
    for( int i = 0; i < RT_COUNT; ++i )
        PP.m_pEffect->SetTexture( PP.m_hTexScene[i], m_pSceneSave[i] );

    //
    // If there are any parameters, initialize them here.
    //
    for( int i = 0; i < NUM_PARAMS; ++i )
    {
        if( PP.m_ahParam[i] != NULL )
            PP.m_pEffect->SetVector( PP.m_ahParam[i], &pStage->avParam[i] );
    }

    // Render the quad
    PP.m_pEffect->SetTechnique( "PostProcess" );

    // Set the vertex declaration
    g_pView->m_pd3dDevice->SetVertexDeclaration( m_pVertDeclPP );

    // Draw the quad
    UINT cPasses, p;
    PP.m_pEffect->Begin( &cPasses, 0 );
    for( p = 0; p < cPasses; ++p )
    {
        bool bUpdateVB = false;     // whether the vertex buffer needs update for this pass

        //
        // If the extents has been modified, the texture coordinates
        // in the quad need to be updated.
        //
        if( aQuad[1].tu != fExtentX )
        {
            aQuad[1].tu = aQuad[3].tu = fExtentX;
            bUpdateVB = true;
        }
        if( aQuad[2].tv != fExtentY )
        {
            aQuad[2].tv = aQuad[3].tv = fExtentY;
            bUpdateVB = true;
        }

        //
        // Check if the pass has annotation for extent info.  Update
        // fScaleX and fScaleY if it does.  Otherwise, default to 1.0f.
        //
        float fScaleX = 1.0f, fScaleY = 1.0f;
        D3DXHANDLE hPass = PP.m_pEffect->GetPass( PP.m_hTPostProcess, p );
        D3DXHANDLE hExtentScaleX = PP.m_pEffect->GetAnnotationByName( hPass, "fScaleX" );
        if( hExtentScaleX != NULL )
            PP.m_pEffect->GetFloat( hExtentScaleX, &fScaleX );
        D3DXHANDLE hExtentScaleY = PP.m_pEffect->GetAnnotationByName( hPass, "fScaleY" );
        if( hExtentScaleY != NULL )
            PP.m_pEffect->GetFloat( hExtentScaleY, &fScaleY );

        //
        // Now modify the quad according to the scaling values specified for
        // this pass
        //
        if( fScaleX != 1.0f )
        {
            aQuad[1].x = ( aQuad[1].x + 0.5f ) * fScaleX - 0.5f;
            aQuad[3].x = ( aQuad[3].x + 0.5f ) * fScaleX - 0.5f;
            bUpdateVB = true;
        }
        if( fScaleY != 1.0f )
        {
            aQuad[2].y = ( aQuad[2].y + 0.5f ) * fScaleY - 0.5f;
            aQuad[3].y = ( aQuad[3].y + 0.5f ) * fScaleY - 0.5f;
            bUpdateVB = true;
        }

        if( bUpdateVB )
        {
            LPVOID pVBData;
            // Scaling requires updating the vertex buffer.
            if( SUCCEEDED( pVB->Lock( 0, 0, &pVBData, D3DLOCK_DISCARD ) ) )
            {
                CopyMemory( pVBData, aQuad, 4 * sizeof(PPVERT) );
                pVB->Unlock();
            }
        }
        fExtentX *= fScaleX;
        fExtentY *= fScaleY;

        // Set up the textures and the render target
        //
        for( int i = 0; i < RT_COUNT; ++i )
        {
            // If this is the very first post-process rendering,
            // obtain the source textures from the scene.
            // Otherwise, initialize the post-process source texture to
            // the previous render target.
            //
            if( m_RTChain[i].m_bFirstRender )
                PP.m_pEffect->SetTexture( PP.m_hTexSource[i], m_pSceneSave[i] );
            else
                PP.m_pEffect->SetTexture( PP.m_hTexSource[i], m_RTChain[i].GetNextSource() );
        }

        //
        // Set up the new render target
        //
        IDirect3DTexture9* pTarget = m_RTChain[PP.m_nRenderTarget].GetNextTarget();
        IDirect3DSurface9* pTexSurf;
        hr = pTarget->GetSurfaceLevel( 0, &pTexSurf );
        if( FAILED( hr ) )
            return E_FAIL;
        g_pView->m_pd3dDevice->SetRenderTarget( 0, pTexSurf );
        pTexSurf->Release();

        // We have output to this render target. Flag it.
        m_RTChain[PP.m_nRenderTarget].m_bFirstRender = false;

        //
        // Clear the render target
        //
        g_pView->m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L );

        //
        // Render
        //
        PP.m_pEffect->BeginPass( p );
        g_pView->m_pd3dDevice->SetStreamSource( 0, pVB, 0, sizeof(PPVERT) );
        g_pView->m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
        PP.m_pEffect->EndPass();

        // Update next rendertarget index
        m_RTChain[PP.m_nRenderTarget].Flip();
    }
    PP.m_pEffect->End();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: PerformPostProcess()
// Desc: Performs the post processing by setting the previous render target
//       as the source texture and rendering a quad with the post process
//       technique set, for each stage.
//       This changes the render target without saving it; the caller has
//       to restore it.
//-----------------------------------------------------------------------------
HRESULT CPostProcess::PerformPostProcess()
{
    HRESULT hr;

    // Extents are used to control how much of the rendertarget is rendered
    // during postprocess. For example, with the extent of 0.5 and 0.5, only
    // the upper left quarter of the rendertarget will be rendered during
    // postprocess.
    float fExtentX = 1.0f, fExtentY = 1.0f;
    UINT uWidth = g_pView->m_d3dsdBackBuffer.Width;
    UINT uHeight = g_pView->m_d3dsdBackBuffer.Height;

    //
    // Set up our quad
    //
    PPVERT Quad[4] =
    {
        { -0.5f,          -0.5f,           1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { uWidth - 0.5f,  -0.5f,           1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f },
        { -0.5f,          uHeight - 0.5f,  1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f },
        { uWidth - 0.5f,  uHeight - 0.5f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f }
    };

    //
    // Create a vertex buffer out of the quad
    //
    IDirect3DVertexBuffer9* pVB;
    hr = g_pView->m_pd3dDevice->CreateVertexBuffer( sizeof(PPVERT) * 4, D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
                                                    0, D3DPOOL_DEFAULT, &pVB, NULL );
    if( FAILED( hr ) )
        return D3DERR_OUTOFVIDEOMEMORY;

    // Fill in the vertex buffer
    LPVOID pVBData;
    if( SUCCEEDED( pVB->Lock( 0, 0, &pVBData, D3DLOCK_DISCARD ) ) )
    {
        CopyMemory( pVBData, Quad, sizeof(Quad) );
        pVB->Unlock();
    }

    // Clear first-time render flags
    for( int i = 0; i < RT_COUNT; ++i )
        m_RTChain[i].m_bFirstRender = true;

    // Perform post processing
    for( UINT i = 0; i < m_cStages; i++ )
    {
        PerformSinglePostProcess( m_aPostProcess[s_asStages[i].nFxIndex], &s_asStages[i], pVB, Quad,
                                  fExtentX, fExtentY );
    }

    // Release the vertex buffer
    pVB->Release();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnCreateDevice()
//-----------------------------------------------------------------------------
HRESULT CPostProcess::OnCreateDevice()
{
    HRESULT hr;

    if( FAILED( hr = D3DXCreateEffectFromResourceW( g_pView->m_pd3dDevice, NULL,
                                                    MAKEINTRESOURCEW( IDR_FX_POSTPROC ), NULL, NULL,
                                                    EFFECT_FLAGS, NULL, &m_pEffect, NULL ) ) )
    {
        return hr;
    }

    for( int i = 0; i < PP_COUNT; i++ )
    {
        if( FAILED( hr = m_aPostProcess[i].Init( EFFECT_FLAGS, CPProcType::s_adwFxFile[i] ) ) )
            return hr;
    }

    m_hTRender = m_pEffect->GetTechniqueByName( "Render" );

    if( FAILED( hr = g_pView->m_pd3dDevice->CreateVertexDeclaration( PPVERT::s_Decl, &m_pVertDeclPP ) ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnResetDevice()
// Desc: Creates the render targets, the size of the back buffer.
//-----------------------------------------------------------------------------
HRESULT CPostProcess::OnResetDevice()
{
    HRESULT hr;

    if( m_pEffect != NULL && FAILED( hr = m_pEffect->OnResetDevice() ) )
        return hr;

    for( int i = 0; i < PP_COUNT; i++ )
    {
        if( FAILED( hr = m_aPostProcess[i].OnResetDevice( g_pView->m_d3dsdBackBuffer.Width,
                                                          g_pView->m_d3dsdBackBuffer.Height ) ) )
        {
            return hr;
        }
    }

    for( int i = 0; i < RT_COUNT; ++i )
    {
        // the scene
        if( FAILED( hr = g_pView->m_pd3dDevice->CreateTexture( g_pView->m_d3dsdBackBuffer.Width,
                                                               g_pView->m_d3dsdBackBuffer.Height,
                                                               1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                                               D3DPOOL_DEFAULT, &m_pSceneSave[i], NULL ) ) )
        {
            return hr;
        }

        // Create the textures for this render target chain
        IDirect3DTexture9* pRT[2];
        ZeroMemory( pRT, sizeof(pRT) );
        for( int t = 0; t < 2; ++t )
        {
            if( FAILED( hr = g_pView->m_pd3dDevice->CreateTexture( g_pView->m_d3dsdBackBuffer.Width,
                                                                   g_pView->m_d3dsdBackBuffer.Height,
                                                                   1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                                                   D3DPOOL_DEFAULT, &pRT[t], NULL ) ) )
            {
                return hr;
            }
        }
        m_RTChain[i].Init( pRT );

        if( pRT[0] != NULL )
        {
            pRT[0]->Release();
            pRT[0] = NULL;
        }
        if( pRT[1] != NULL )
        {
            pRT[1]->Release();
            pRT[1] = NULL;
        }
    }

    switch( m_nPasses )
    {
        case 1:
        case 2:
        case 3:
            m_pSceneSave[0]->GetSurfaceLevel( 0, &m_pSceneSurface );
            break;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnLostDevice()
//-----------------------------------------------------------------------------
void CPostProcess::OnLostDevice()
{
    if( m_pEffect != NULL )
        m_pEffect->OnLostDevice();

    for( int i = 0; i < PP_COUNT; i++ )
        m_aPostProcess[i].OnLostDevice();

    for( int i = 0; i < RT_COUNT; ++i )
    {
        if( m_pSceneSave[i] != NULL )
        {
            m_pSceneSave[i]->Release();
            m_pSceneSave[i] = NULL;
        }
        m_RTChain[i].Cleanup();
    }

    if( m_pSceneSurface != NULL )
    {
        m_pSceneSurface->Release();
        m_pSceneSurface = NULL;
    }
}




//-----------------------------------------------------------------------------
// Name: OnDestroyDevice()
//-----------------------------------------------------------------------------
void CPostProcess::OnDestroyDevice()
{
    if( m_pEffect != NULL )
    {
        m_pEffect->Release();
        m_pEffect = NULL;
    }

    if( m_pVertDeclPP != NULL )
    {
        m_pVertDeclPP->Release();
        m_pVertDeclPP = NULL;
    }

    for( int i = 0; i < PP_COUNT; i++ )
        m_aPostProcess[i].Cleanup();
}
