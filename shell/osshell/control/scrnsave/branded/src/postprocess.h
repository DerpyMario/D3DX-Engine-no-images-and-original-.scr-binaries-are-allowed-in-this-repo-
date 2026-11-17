//-----------------------------------------------------------------------------
// File: postprocess.h
//
// Desc: The glow of the flare: the flare scene is drawn into a texture,
//       which a chain of full screen post processing effects (PP_*.fx)
//       shrinks, blurs ("bloom") and enlarges again; PostProc.fx finally
//       adds the result, tinted with g_cTargetColor, onto the back buffer.
//
//       This is the post processing framework of the DirectX SDK sample
//       "PostProcess", turned into classes: CPProcType is the sample's
//       CPostProcess (one effect), PPROCSTAGE its CPProcInstance (one use of
//       an effect, with its parameters), and CPostProcess the sample's
//       globals and functions.  Only one render target channel (the colour)
//       is used, and the stages are fixed (s_asStages).
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\postprocess.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_POSTPROCESS_H
#define _BRANDED_POSTPROCESS_H

#include "vars.h"

#define NUM_PARAMS      2       // parameters per post processing effect
#define RT_COUNT        1       // render target channels (colour only)
#define PP_COUNT        6       // post processing effects (s_adwFxFile)
#define PP_STAGES       6       // stages of the chain (s_asStages)

// the post processing effects, indices into CPostProcess::m_aPostProcess
enum
{
    PP_COLORBLOOMH = 0,
    PP_COLORGBLURV,
    PP_COLORBRIGHTPASS,         // (not used)
    PP_COLORDOWNFILTER4,
    PP_COLORUPFILTER,
    PP_COLORCOMBINE4,
};


//-----------------------------------------------------------------------------
// Name: struct PPVERT
// Desc: Vertex of the full screen quads.
//
//       (The original declares it as an unnamed struct with a typedef name,
//       so that the symbol of s_Decl is "<unnamed-tag>::s_Decl".)
//-----------------------------------------------------------------------------
struct PPVERT
{
    FLOAT x, y, z, rhw;
    FLOAT tu, tv;               // texture coordinates of the post process source
    FLOAT tu2, tv2;             // ... and of the original scene

    static const D3DVERTEXELEMENT9 s_Decl[4];
};


//-----------------------------------------------------------------------------
// Name: struct PPROCSTAGE
// Desc: One stage of the post processing chain.
//-----------------------------------------------------------------------------
struct PPROCSTAGE
{
    D3DXVECTOR4     avParam[NUM_PARAMS];    // values of the effect's parameters
    int             nFxIndex;               // the effect, PP_*
};


//-----------------------------------------------------------------------------
// Name: class CPProcType
// Desc: One post processing effect (a PP_*.fx resource).  The effect
//       describes itself with annotations of its technique "PostProcess":
//       nRenderTarget, Parameter<n>, Parameter<n>Desc, Parameter<n>Size and
//       Parameter<n>Def; a parameter with the annotation
//       ConvertPixelsToTexels gets the named pixel offsets converted to
//       texel offsets.
//-----------------------------------------------------------------------------
class CPProcType
{
public:
                    CPProcType();
                    ~CPProcType();

    HRESULT         Init( DWORD dwShaderFlags, DWORD dwResourceID );

    void            Cleanup()
    {
        if( m_pEffect != NULL )
        {
            m_pEffect->Release();
            m_pEffect = NULL;
        }
    }

    void            OnLostDevice()
    {
        if( m_pEffect != NULL )
            m_pEffect->OnLostDevice();
    }

    HRESULT         OnResetDevice( DWORD dwWidth, DWORD dwHeight );

public:
    ID3DXEffect*    m_pEffect;
    D3DXHANDLE      m_ahParam[NUM_PARAMS];
    D3DXHANDLE      m_hTexSource[4];            // g_txSrc*: the result of the last stage
    D3DXHANDLE      m_hTexScene[4];             // g_txScene*: the original scene
    D3DXHANDLE      m_hTPostProcess;
    int             m_nRenderTarget;            // the render target channel written
    WCHAR           m_awszParamName[NUM_PARAMS][MAX_PATH];
    WCHAR           m_awszParamDesc[NUM_PARAMS][MAX_PATH];
    int             m_anParamSize[NUM_PARAMS];
    D3DXVECTOR4     m_avParamDef[NUM_PARAMS];
    bool            m_bWrite[4];

    static DWORD    s_adwFxFile[PP_COUNT];      // the effect resources, by PP_*
};


//-----------------------------------------------------------------------------
// Name: class CRenderTargetChain
// Desc: Two render targets that the stages render into by turns.
//-----------------------------------------------------------------------------
class CRenderTargetChain
{
public:
    CRenderTargetChain() : m_bFirstRender( true ), m_nNext( 0 )
    {
        ZeroMemory( m_pRenderTarget, sizeof(m_pRenderTarget) );
    }

    ~CRenderTargetChain() { Cleanup(); }

    void Init( IDirect3DTexture9** pRT )
    {
        for( int i = 0; i < 2; ++i )
        {
            m_pRenderTarget[i] = pRT[i];
            m_pRenderTarget[i]->AddRef();
        }
    }

    void Cleanup()
    {
        if( m_pRenderTarget[0] != NULL )
        {
            m_pRenderTarget[0]->Release();
            m_pRenderTarget[0] = NULL;
        }
        if( m_pRenderTarget[1] != NULL )
        {
            m_pRenderTarget[1]->Release();
            m_pRenderTarget[1] = NULL;
        }
    }

    void Flip()                             { m_nNext = 1 - m_nNext; }

    IDirect3DTexture9* GetPrevTarget()      { return m_pRenderTarget[1 - m_nNext]; }
    IDirect3DTexture9* GetPrevSource()      { return m_pRenderTarget[m_nNext]; }
    IDirect3DTexture9* GetNextTarget()      { return m_pRenderTarget[m_nNext]; }
    IDirect3DTexture9* GetNextSource()      { return m_pRenderTarget[1 - m_nNext]; }

public:
    bool                m_bFirstRender;     // nothing rendered into the chain yet
    int                 m_nNext;
    IDirect3DTexture9*  m_pRenderTarget[2];
};


//-----------------------------------------------------------------------------
// Name: class CPostProcess
// Desc: Runs the post processing chain.
//-----------------------------------------------------------------------------
class CPostProcess
{
public:
                    CPostProcess();
                    ~CPostProcess();

    HRESULT         OnCreateDevice();
    HRESULT         OnResetDevice();
    void            OnLostDevice();
    void            OnDestroyDevice();

    void            OnFrameRender( IDirect3DSurface9* pBackBuffer );
    HRESULT         PerformPostProcess();

    void            SetBloomScale( FLOAT fScale )
    {
        m_fBloomScale = fScale;
        s_asStages[2].avParam[0].x = m_fBloomScale;
        s_asStages[3].avParam[0].x = m_fBloomScale;
    }

private:
    HRESULT         PerformSinglePostProcess( CPProcType& PP, PPROCSTAGE* pStage, IDirect3DVertexBuffer9* pVB,
                                              PPVERT* aQuad, FLOAT& fExtentX, FLOAT& fExtentY );

public:
    ID3DXEffect*                    m_pEffect;              // PostProc.fx
    IDirect3DVertexDeclaration9*    m_pVertDeclPP;
    int                             m_iReserved;            // cleared, never used
    CPProcType                      m_aPostProcess[PP_COUNT];
    D3DXHANDLE                      m_hTRender;             // technique "Render" of PostProc.fx
    IDirect3DTexture9*              m_pSceneSave[RT_COUNT]; // the scene is rendered into this
    CRenderTargetChain              m_RTChain[RT_COUNT];
    UINT                            m_cStages;
    int                             m_nPasses;              // (always 1)
    int                             m_nRtUsed;              // (always 1)
    IDirect3DSurface9*              m_pSceneSurface;        // level 0 of m_pSceneSave[0]
    FLOAT                           m_fBloomScale;

private:
    static PPROCSTAGE               s_asStages[PP_STAGES];
};

#if defined(_WIN64)
static_assert( sizeof(PPVERT) == 0x20, "PPVERT layout" );
static_assert( sizeof(PPROCSTAGE) == 0x24, "PPROCSTAGE layout" );
static_assert( sizeof(CPProcType) == 0x8b0, "CPProcType layout" );
static_assert( sizeof(CRenderTargetChain) == 0x18, "CRenderTargetChain layout" );
static_assert( sizeof(CPostProcess) == 0x3480, "CPostProcess layout" );
#endif

#endif // _BRANDED_POSTPROCESS_H
