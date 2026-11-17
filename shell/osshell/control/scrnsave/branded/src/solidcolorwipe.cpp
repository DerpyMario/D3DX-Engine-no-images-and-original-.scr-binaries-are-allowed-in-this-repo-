//-----------------------------------------------------------------------------
// File: solidcolorwipe.cpp
//
// Desc: CSolidColorWipe - see solidcolorwipe.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\solidcolorwipe.obj
//-----------------------------------------------------------------------------
#include <new>
#include "solidcolorwipe.h"
#include "ssview.h"
#include "resource.h"

SCWIPEPARMS CSolidColorWipe::s_asWipeParms[SCWIPE_COUNT] =
{
    //  start  interval  duration  width  wipe map
    {   2.0f,  10.0f,    9.0f,     0.5f,  IDR_TGA_WIPE0 },
    {   0.0f,  10.0f,    9.0f,     0.5f,  IDR_TGA_WIPE1 },
    {   0.0f,  10.0f,    9.0f,     0.5f,  IDR_TGA_WIPE2 },
    {   0.0f,  10.0f,    9.0f,     0.5f,  IDR_TGA_WIPE3 },
    {   0.0f,  10.0f,    9.0f,     0.5f,  IDR_TGA_WIPE4 },
};

DWORD CSolidColorWipe::s_acTargetColors[SCWIPE_TARGET_COLORS] =
{
    0xff098dd1,
    0xff4ea69c,
    0xff02a99b,
    0xff289c65,
    0xff215fa1,
    0xff645d9b,
    0xff9e8fd8,
};

// progress of the colour blend, 0 .. 1
static FLOAT s_fBlend;




//-----------------------------------------------------------------------------
// Name: CSolidColorWipe()
//-----------------------------------------------------------------------------
CSolidColorWipe::CSolidColorWipe()
{
    m_ppWipeMaps = NULL;
    m_eColorOrder = ORDER_CYCLE;
    m_bForward = true;
    m_bFirstFrame = true;
    m_cWipes = SCWIPE_COUNT;
    m_iWipe = 0;
    m_cColors = SCWIPE_TARGET_COLORS;
    m_iColor = 0;

    m_cPrevColor = D3DXCOLOR( s_acTargetColors[0] );
    m_cPrevColor.r *= SCWIPE_COLOR_SCALE;
    m_cPrevColor.g *= SCWIPE_COLOR_SCALE;
    m_cPrevColor.b *= SCWIPE_COLOR_SCALE;
    m_cNextColor = m_cPrevColor;
}




//-----------------------------------------------------------------------------
// Name: ~CSolidColorWipe()
//-----------------------------------------------------------------------------
CSolidColorWipe::~CSolidColorWipe()
{
    FinalCleanup();
}




//-----------------------------------------------------------------------------
// Name: Init()
//-----------------------------------------------------------------------------
HRESULT CSolidColorWipe::Init()
{
    HRESULT hr = S_OK;

    m_ppWipeMaps = new( std::nothrow ) IDirect3DTexture9*[m_cWipes];
    if( m_ppWipeMaps != NULL )
    {
        ZeroMemory( m_ppWipeMaps, m_cWipes * sizeof(IDirect3DTexture9*) );
        hr = m_Wipe.Init();
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: FinalCleanup()
//-----------------------------------------------------------------------------
void CSolidColorWipe::FinalCleanup()
{
    OnLostDevice();
    OnDestroyDevice();

    if( m_ppWipeMaps != NULL )
    {
        delete[] m_ppWipeMaps;
        m_ppWipeMaps = NULL;
    }
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
// Desc: Starts a wipe to the next colour every s_asWipeParms[].fInterval
//       seconds, and blends g_cTargetColor along.
//-----------------------------------------------------------------------------
void CSolidColorWipe::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
{
    int cTries;

    if( m_bFirstFrame )
    {
        // as if the last wipe had ended a second ago
        m_fWipeStart = fTime - s_asWipeParms[m_iWipe].fDuration - 1.0f;
        m_fNextWipe = fTime + s_asWipeParms[m_iWipe].fStart;
        m_bFirstFrame = false;
    }

    if( fTime - m_fWipeStart <= s_asWipeParms[m_iWipe].fDuration )
        s_fBlend = ( fTime - m_fWipeStart ) / s_asWipeParms[m_iWipe].fDuration;

    if( fTime >= m_fNextWipe )
    {
        UINT iColor;
        UINT iWipe;

        //
        // the next colour
        //
        switch( m_eColorOrder )
        {
            case ORDER_RANDOM:
                cTries = 100;
                do
                {
                    iColor = (UINT)( RandFloat( 0.0f, (FLOAT)( m_cColors - 1 ) ) + 0.5 );
                }
                while( --cTries > 0 && iColor == m_iColor );
                break;

            case ORDER_CYCLE:
                iColor = ( m_iColor + 1 ) % ( m_cColors - 1 );
                break;

            case ORDER_PINGPONG:
                if( m_bForward )
                {
                    if( m_iColor < m_cColors - 1 )
                    {
                        iColor = m_iColor + 1;
                    }
                    else
                    {
                        iColor = m_cColors - 2;
                        m_bForward = false;
                    }
                }
                else
                {
                    if( m_iColor == 0 )
                    {
                        iColor = 1;
                        m_bForward = true;
                    }
                    else
                    {
                        iColor = m_iColor - 1;
                    }
                }
                iColor %= m_cColors;
                break;

            default:
                iColor = 0;
                break;
        }

        m_iColor = iColor;
        s_fBlend = 0.0f;

        m_cPrevColor = m_cNextColor;
        m_cNextColor = D3DXCOLOR( s_acTargetColors[m_iColor] );
        m_cNextColor.r *= SCWIPE_COLOR_SCALE;
        m_cNextColor.g *= SCWIPE_COLOR_SCALE;
        m_cNextColor.b *= SCWIPE_COLOR_SCALE;

        //
        // the next wipe map (another one than the last time)
        //
        cTries = 100;
        do
        {
            iWipe = (UINT)( RandFloat( 0.0f, (FLOAT)( m_cWipes - 1 ) ) + 0.5 );
        }
        while( --cTries > 0 && iWipe == m_iWipe );
        m_iWipe = iWipe;

        WIPEPARMS wp;
        wp.fDuration = s_asWipeParms[m_iWipe].fDuration;
        wp.fWidth = s_asWipeParms[m_iWipe].fWidth;
        wp.ppSrc0 = NULL;
        wp.ppSrc1 = NULL;
        wp.ppWipeMap = &m_ppWipeMaps[m_iWipe];
        wp.cColor0 = m_cPrevColor;
        wp.cColor1 = m_cNextColor;

        memcpy( &m_Wipe.m_WipeParms, &wp, sizeof(WIPEPARMS) );
        m_Wipe.m_eState = CAnimation::STATE_LOADED;
        m_Wipe.Setup();
        m_Wipe.Go( fTime );

        m_fWipeStart = m_fNextWipe;
        m_fNextWipe += s_asWipeParms[m_iWipe].fInterval;
    }

    g_cTargetColor.r = s_fBlend * m_cNextColor.r + ( 1.0f - s_fBlend ) * m_cPrevColor.r;
    g_cTargetColor.g = s_fBlend * m_cNextColor.g + ( 1.0f - s_fBlend ) * m_cPrevColor.g;
    g_cTargetColor.b = s_fBlend * m_cNextColor.b + ( 1.0f - s_fBlend ) * m_cPrevColor.b;

    m_Wipe.OnFrameMove( fTime, fElapsedTime, mParent );
}




//-----------------------------------------------------------------------------
// Name: OnFrameRender()
//-----------------------------------------------------------------------------
void CSolidColorWipe::OnFrameRender( FLOAT fTime, FLOAT fElapsedTime )
{
    m_Wipe.OnFrameRender( fTime, fElapsedTime );
}




//-----------------------------------------------------------------------------
// Name: OnCreateDevice()
//-----------------------------------------------------------------------------
HRESULT CSolidColorWipe::OnCreateDevice()
{
    HRESULT hr;

    if( FAILED( hr = m_Wipe.OnCreateDevice() ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnResetDevice()
// Desc: (Re)loads the wipe maps, which live in the default pool.
//-----------------------------------------------------------------------------
HRESULT CSolidColorWipe::OnResetDevice()
{
    HRESULT hr;

    for( UINT i = 0; i < m_cWipes && m_ppWipeMaps != NULL; i++ )
    {
        if( m_ppWipeMaps[i] != NULL )
        {
            m_ppWipeMaps[i]->Release();
            m_ppWipeMaps[i] = NULL;
        }

        if( FAILED( hr = MSLoadTextureFromResource( g_pView->m_pd3dDevice,
                                                    MAKEINTRESOURCEW( (WORD)s_asWipeParms[i].idWipeMap ),
                                                    RT_RCDATA, &m_ppWipeMaps[i] ) ) )
        {
            return hr;
        }
    }

    if( FAILED( hr = m_Wipe.OnResetDevice() ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnLostDevice()
//-----------------------------------------------------------------------------
void CSolidColorWipe::OnLostDevice()
{
    for( UINT i = 0; i < m_cWipes && m_ppWipeMaps != NULL; i++ )
    {
        if( m_ppWipeMaps[i] != NULL )
        {
            m_ppWipeMaps[i]->Release();
            m_ppWipeMaps[i] = NULL;
        }
    }

    m_Wipe.OnLostDevice();
}




//-----------------------------------------------------------------------------
// Name: OnDestroyDevice()
//-----------------------------------------------------------------------------
void CSolidColorWipe::OnDestroyDevice()
{
    for( UINT i = 0; i < m_cWipes && m_ppWipeMaps != NULL; i++ )
    {
        if( m_ppWipeMaps[i] != NULL )
        {
            m_ppWipeMaps[i]->Release();
            m_ppWipeMaps[i] = NULL;
        }
    }

    m_Wipe.OnDestroyDevice();
}
