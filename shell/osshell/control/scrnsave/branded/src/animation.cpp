//-----------------------------------------------------------------------------
// File: animation.cpp
//
// Desc: CAnimation - key frame animation (see animation.h).
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\animation.obj
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <new>
#include "animation.h"
#include "resourcefile.h"
#include "d3dsaver9.h"          // D3DAPPERR_MEDIANOTFOUND

#define MAX_LINE    256




//-----------------------------------------------------------------------------
// Name: CAnimation()
//-----------------------------------------------------------------------------
CAnimation::CAnimation()
{
    m_eType = KEY_NONE;
    m_eState = STATE_EMPTY;
    m_bReverse = FALSE;
    m_eLoop = LOOP_ONCE;
    m_fDelay = 0.0f;
    m_fStartTime = -1.0f;
    m_iKey = 0;
    m_iNextKey = 1;
    m_cKeys = 0;
    m_fLength = 0.0f;
    m_pKeys = NULL;
    m_pCurKeys = NULL;
    m_pFwdKeys = NULL;
    m_pRevKeys = NULL;
    m_pLastKey = NULL;
    m_pCurLastKey = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CAnimation()
//-----------------------------------------------------------------------------
CAnimation::~CAnimation()
{
    CAnimation::FinalCleanup();
}




//-----------------------------------------------------------------------------
// Name: Init()
// Desc: Rewinds the animation to its first key; it does not move yet.
//-----------------------------------------------------------------------------
void CAnimation::Init( FLOAT fTime )
{
    m_iKey = 0;
    m_fStartTime = fTime;
    m_iNextKey = 1;
    m_eState = STATE_STOPPED;
}




//-----------------------------------------------------------------------------
// Name: Go()
// Desc: Starts the animation: a stopped one continues, anything else starts
//       over.
//-----------------------------------------------------------------------------
void CAnimation::Go( FLOAT fTime )
{
    m_fStartTime = fTime;

    switch( m_eState )
    {
        case STATE_STOPPED:
            m_eState = STATE_PLAYING;
            break;

        case STATE_LOADED:
        case STATE_PLAYING:
        case STATE_DONE:
            Init( fTime );
            m_eState = STATE_PLAYING;
            break;

        default:
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: Stop()
//-----------------------------------------------------------------------------
void CAnimation::Stop( FLOAT /*fTime*/ )
{
    if( m_eState == STATE_PLAYING || m_eState == STATE_DONE )
        m_eState = STATE_STOPPED;
}




//-----------------------------------------------------------------------------
// Name: GetLength()
//-----------------------------------------------------------------------------
FLOAT CAnimation::GetLength()
{
    if( m_pCurLastKey != NULL )
        return m_pCurLastKey->fTime;

    return 0.0f;
}




//-----------------------------------------------------------------------------
// Name: FinalCleanup()
//-----------------------------------------------------------------------------
void CAnimation::FinalCleanup()
{
    if( m_pKeys != NULL )
    {
        delete[] m_pKeys;
        m_pKeys = NULL;
    }
    if( m_pFwdKeys != NULL )
    {
        delete[] m_pFwdKeys;
        m_pFwdKeys = NULL;
    }
    if( m_pRevKeys != NULL )
    {
        delete[] m_pRevKeys;
        m_pRevKeys = NULL;
    }
    m_pCurKeys = NULL;
}




//-----------------------------------------------------------------------------
// Name: Load()
// Desc: Loads the key frames from a RT_RCDATA resource of the executable.
//-----------------------------------------------------------------------------
HRESULT CAnimation::Load( DWORD dwResourceID )
{
    HRESULT        hr = S_OK;
    char           szLine[MAX_LINE];
    size_t         cchLine;
    UINT           i;
    CResourceFile* pFile = new( std::nothrow ) CResourceFile;

    if( pFile == NULL )
    {
        hr = E_OUTOFMEMORY;
        goto LFail;
    }

    if( !pFile->Open( MAKEINTRESOURCEW( LOWORD( dwResourceID ) ), RT_RCDATA ) )
    {
        hr = D3DAPPERR_MEDIANOTFOUND;
        goto LFail;
    }

    // the length
    if( pFile->GetString( szLine, MAX_LINE, &cchLine ) == NULL ||
        sscanf_s( szLine, "%f", &m_fLength ) != 1 ||
        m_fLength < 0.0f )
    {
        hr = E_FAIL;
        goto LFail;
    }

    // the type of the keys
    if( pFile->GetString( szLine, MAX_LINE, &cchLine ) == NULL )
    {
        hr = E_FAIL;
        goto LFail;
    }

    if( strncmp( szLine, "PosUp", 4 ) == 0 )
        m_eType = KEY_POSUP;
    else if( strncmp( szLine, "RotPos", 5 ) == 0 )
        m_eType = KEY_ROTPOS;
    else if( strncmp( szLine, "XForm", 4 ) == 0 )
        m_eType = KEY_XFORM;
    else
    {
        hr = E_FAIL;
        goto LFail;
    }

    // the number of keys
    if( pFile->GetString( szLine, MAX_LINE, &cchLine ) == NULL ||
        sscanf_s( szLine, "%d", &m_cKeys ) != 1 ||
        (int)m_cKeys < 0 ||
        m_cKeys > ANIM_MAX_KEYS )
    {
        hr = E_FAIL;
        goto LFail;
    }

    // the keys
    m_pKeys = new( std::nothrow ) ANIM_KEY[m_cKeys];
    if( m_pKeys == NULL )
    {
        hr = E_OUTOFMEMORY;
    }
    else
    {
        ANIM_KEY* pKey = m_pKeys;

        for( i = 0; i < m_cKeys; i++, pKey++ )
        {
            if( pFile->GetString( szLine, MAX_LINE, &cchLine ) == NULL )
            {
                hr = E_FAIL;
                break;
            }

            memcpy( pKey, &g_sDefaultAnimKey, sizeof(ANIM_KEY) );

            if( FAILED( hr = LoadKey( pKey, MAX_LINE, szLine, &cchLine ) ) )
                goto LFail;
        }

        if( FAILED( hr ) )
            goto LFail;

        // stretch the key times to the length of the animation
        m_pLastKey = &m_pKeys[m_cKeys - 1];

        FLOAT fScale = m_fLength / m_pLastKey->fTime;

        for( i = 0; i < m_cKeys; i++ )
            m_pKeys[i].fTime = fScale * m_pKeys[i].fTime;
    }

    if( FAILED( hr ) )
        goto LFail;

    // the keys forwards ...
    m_pFwdKeys = new( std::nothrow ) ANIM_KEY[m_cKeys];
    if( m_pFwdKeys == NULL )
    {
        hr = E_OUTOFMEMORY;
    }
    else
    {
        for( i = 0; i < m_cKeys; i++ )
            memcpy( &m_pFwdKeys[i], &m_pKeys[i], sizeof(ANIM_KEY) );

        m_pCurKeys = m_pFwdKeys;
        m_pCurLastKey = &m_pFwdKeys[m_cKeys - 1];
    }

    if( FAILED( hr ) )
        goto LFail;

    // ... and backwards
    m_pRevKeys = new( std::nothrow ) ANIM_KEY[m_cKeys];
    if( m_pRevKeys == NULL )
    {
        hr = E_OUTOFMEMORY;
    }
    else
    {
        for( i = 0; i < m_cKeys; i++ )
        {
            memcpy( &m_pRevKeys[i], &m_pFwdKeys[m_cKeys - i - 1], sizeof(ANIM_KEY) );
            m_pRevKeys[i].fTime = m_pCurLastKey->fTime - m_pRevKeys[i].fTime;
        }

        m_eState = STATE_LOADED;
    }

    if( FAILED( hr ) )
        goto LFail;

    PostProcess();
    goto LDone;

LFail:
    CAnimation::FinalCleanup();

LDone:
    if( pFile != NULL )
        delete pFile;

    return hr;
}




//-----------------------------------------------------------------------------
// Name: LoadKey()
// Desc: Parses one key; the derived classes know the formats.
//-----------------------------------------------------------------------------
HRESULT CAnimation::LoadKey( ANIM_KEY* /*pKey*/, int /*cchLine*/, char* /*pszLine*/, size_t* /*pcchLine*/ )
{
    return E_FAIL;
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
// Desc: Advances the animation and returns its transformation.
//-----------------------------------------------------------------------------
void CAnimation::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mOut )
{
    D3DXQUATERNION qRot = m_pCurKeys[m_iKey].qRot;
    D3DXVECTOR3    vPos = m_pCurKeys[m_iKey].vPos;

    if( m_eState == STATE_STOPPED )
    {
        // hold still
        m_fStartTime = fElapsedTime + m_fStartTime;
    }
    else if( m_eState == STATE_PLAYING )
    {
        if( m_fDelay <= 0.0f )
        {
            Animate( fTime, fElapsedTime, qRot, vPos );
        }
        else
        {
            // not yet
            m_fStartTime = fTime;
            m_fDelay = m_fDelay - fElapsedTime;
        }
    }

    D3DXMATRIX mRot, mTrans;

    D3DXMatrixRotationQuaternion( &mRot, &qRot );
    D3DXMatrixTranslation( &mTrans, vPos.x, vPos.y, vPos.z );
    mOut = mRot * mTrans;
}




//-----------------------------------------------------------------------------
// Name: Animate()
// Desc: Finds the keys around the current time (looping or reversing at the
//       end) and interpolates between them.
//-----------------------------------------------------------------------------
void CAnimation::Animate( FLOAT fTime, FLOAT /*fElapsedTime*/, D3DXQUATERNION& qRot, D3DXVECTOR3& vPos )
{
    UINT  cKeys = m_cKeys;
    UINT  iLastKey = cKeys - 1;
    FLOAT t = fTime - m_fStartTime;

    while( m_iKey < cKeys - 1 )
    {
        if( t <= m_pCurKeys[m_iNextKey].fTime )
            break;
        m_iKey++;
        m_iNextKey++;
    }

    if( m_iNextKey >= cKeys )
    {
        // the end has been reached
        m_fDelay = 0.0f;

        switch( m_eLoop )
        {
            case LOOP_ONCE:
                m_eState = STATE_DONE;
                m_iNextKey = m_iKey;
                goto LDone;

            case LOOP_REPEAT:
                break;

            case LOOP_PINGPONG:
                m_bReverse = !m_bReverse;

                if( m_bReverse == FALSE )
                    m_pCurKeys = m_pFwdKeys;
                else if( m_bReverse == TRUE )
                    m_pCurKeys = m_pRevKeys;

                if( m_pCurKeys != NULL )
                    m_pCurLastKey = &m_pCurKeys[iLastKey];
                break;

            default:
                goto LDone;
        }

        // start over
        while( t > m_pCurLastKey->fTime )
        {
            FLOAT fLength = m_pCurLastKey->fTime;

            m_fStartTime = fLength + m_fStartTime;
            t = t - fLength;
        }

        m_iKey = 0;
        m_iNextKey = 1;

        if( iLastKey != 0 )
        {
            do
            {
                if( t <= m_pCurKeys[m_iNextKey].fTime )
                    break;
                m_iKey++;
                m_iNextKey++;
            }
            while( m_iKey < cKeys - 1 );
        }
    }

LDone:
    if( m_eState == STATE_DONE )
    {
        qRot = m_pCurLastKey->qRot;
        vPos = m_pCurLastKey->vPos;
    }
    else
    {
        ANIM_KEY* pKey = &m_pCurKeys[m_iKey];

        DoTween( pKey, pKey + 1, ( t - pKey->fTime ) / ( pKey[1].fTime - pKey->fTime ), qRot, vPos );
    }
}
