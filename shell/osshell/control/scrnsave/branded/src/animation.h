//-----------------------------------------------------------------------------
// File: animation.h
//
// Desc: CAnimation, a key frame animation that produces a transformation
//       matrix.  The key frames come from a text resource (RT_RCDATA):
//
//           <length in seconds>
//           <key type: PosUp | RotPos | XForm>
//           <number of keys>
//           <key> ...
//
//       The keys are read by LoadKey() of the derived classes; their times
//       are frame numbers at 30 frames per second, which Load() scales so
//       that the last key falls on the length of the animation.
//
//       Original objects:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\animation.obj
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\vars.obj (g_sDefaultAnimKey)
//-----------------------------------------------------------------------------
#ifndef _BRANDED_ANIMATION_H
#define _BRANDED_ANIMATION_H

#include "vars.h"

#define ANIM_FRAMES_PER_SECOND  30.0f
#define ANIM_MAX_KEYS           1000

//-----------------------------------------------------------------------------
// Name: struct ANIM_KEY
//-----------------------------------------------------------------------------
struct ANIM_KEY
{
    FLOAT           fTime;
    D3DXQUATERNION  qRot;
    D3DXVECTOR3     vPos;
    FLOAT           afReserved[16];     // never read by the screen saver
};

// The value every key starts with before LoadKey() fills it in
extern ANIM_KEY g_sDefaultAnimKey;

//-----------------------------------------------------------------------------
// Name: class CAnimation
//-----------------------------------------------------------------------------
class CAnimation
{
public:
    enum State
    {
        STATE_EMPTY = 0,
        STATE_LOADED,           // ready, not started
        STATE_STOPPED,          // stopped (time does not advance)
        STATE_PLAYING,
        STATE_DONE              // a one-shot animation reached its end
    };

    enum LoopMode
    {
        LOOP_ONCE = 0,
        LOOP_REPEAT,
        LOOP_PINGPONG
    };

    enum KeyType
    {
        KEY_NONE = -1,
        KEY_POSUP = 0,          // position and up vector (not supported by the loaders here)
        KEY_ROTPOS,             // quaternion and position
        KEY_XFORM               // 4x4 matrix
    };

public:
                    CAnimation();
    virtual         ~CAnimation();

    virtual void    Init( FLOAT fTime );
    virtual void    Go( FLOAT fTime );
    virtual void    Stop( FLOAT fTime );
    virtual FLOAT   GetLength();
    virtual HRESULT Load( DWORD dwResourceID );
    virtual void    Randomize()                                     {}
    virtual void    Randomize( FLOAT /*fScale*/, FLOAT /*fAmount*/ ) {}
    virtual void    PostProcess()                                   {}
    virtual void    OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mOut );

protected:
    virtual HRESULT LoadKey( ANIM_KEY* pKey, int cchLine, char* pszLine, size_t* pcchLine );
    virtual void    FinalCleanup();
    virtual void    DoTween( ANIM_KEY* /*pKey0*/, ANIM_KEY* /*pKey1*/, FLOAT /*fLerp*/,
                             D3DXQUATERNION& /*qRot*/, D3DXVECTOR3& /*vPos*/ ) {}

    void            Animate( FLOAT fTime, FLOAT fElapsedTime, D3DXQUATERNION& qRot, D3DXVECTOR3& vPos );

public:
    State           m_eState;
    BOOL            m_bReverse;         // ping-pong: running backwards
    LoopMode        m_eLoop;
    KeyType         m_eType;
    FLOAT           m_fDelay;           // seconds before the animation starts moving
    FLOAT           m_fStartTime;
    UINT            m_iKey;             // the keys around the current time
    UINT            m_iNextKey;
    FLOAT           m_afReserved[5];    // never used
    UINT            m_cKeys;
    FLOAT           m_fLength;          // seconds
    ANIM_KEY*       m_pKeys;            // as loaded
    ANIM_KEY*       m_pCurKeys;         // m_pFwdKeys or m_pRevKeys
    ANIM_KEY*       m_pFwdKeys;
    ANIM_KEY*       m_pRevKeys;         // reversed, for ping-pong
    ANIM_KEY*       m_pLastKey;         // last of m_pKeys
    ANIM_KEY*       m_pCurLastKey;      // last of m_pCurKeys
};

#if defined(_WIN64)
static_assert( sizeof(ANIM_KEY) == 0x60, "ANIM_KEY layout" );
static_assert( sizeof(CAnimation) == 0x78, "CAnimation layout" );
#endif

#endif // _BRANDED_ANIMATION_H
