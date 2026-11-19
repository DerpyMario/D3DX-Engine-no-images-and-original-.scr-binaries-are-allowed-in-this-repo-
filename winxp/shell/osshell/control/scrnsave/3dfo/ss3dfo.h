//-----------------------------------------------------------------------------
// File: ss3dfo.h
//
// Desc: 3D Flying Objects screen saver (ss3dfo.scr) - the Direct3D 8 version
//       of the OpenGL screen saver of Windows NT, as shipped with Windows XP
//       and Windows Server 2003 (x86).
//
//       An object (the Windows logo, an exploding sphere, one or two
//       ribbons, a drop splashing into water, a twisting shape or a waving
//       textured flag) flies in a window that bounces around the screen.
//
//       Unlike the other Direct3D screen savers, the binary has no symbol
//       file: all the names are chosen by the reconstruction, after the
//       OpenGL version where it is known (mesh.c, the style modules, the
//       texture helpers of the screen saver common library).
//
//       Build output: ss3dfo.scr, in
//       d:\nt\shell\osshell\control\scrnsave\3dfo\obj\i386 (installed as
//       wss3dfo.scr on x64).
//-----------------------------------------------------------------------------
#ifndef _SS3DFO_H
#define _SS3DFO_H

#include "../d3dsaver/d3dsaver.h"
#include <d3dx8.h>

#ifndef PI
#define PI                      3.14159265358979323846f
#endif

// The OpenGL constants the material functions take
#define GL_FRONT                0x0404
#define GL_FRONT_AND_BACK       0x0408
#define GL_SPECULAR             0x1202
#define GL_SHININESS            0x1601
#define GL_AMBIENT_AND_DIFFUSE  0x1602

// Random number in [0, 1]
#define FRAND()                 ( (FLOAT)rand() * (1.0f / RAND_MAX) )


//-----------------------------------------------------------------------------
// Styles
//-----------------------------------------------------------------------------
enum
{
    STYLE_WINLOGO = 0,
    STYLE_EXPLODE,
    STYLE_RIBBON,
    STYLE_2RIBBONS,
    STYLE_SPLASH,
    STYLE_TWIST,
    STYLE_FLAG,
    NUM_STYLES
};

// Which settings a style uses (gulStyleOptions)
#define OPT_COLOR_CYCLE         0x0001
#define OPT_SMOOTH_SHADE        0x0002
#define OPT_TESSEL              0x0008
#define OPT_SIZE                0x0010
#define OPT_TEXTURE             0x0020

// Flags the update functions get
#define MF_CYCLE_COLORS         0x0002
#define MF_TWO_RIBBONS          0x0004


//-----------------------------------------------------------------------------
// Meshes (mesh.cpp)
//-----------------------------------------------------------------------------
#define MAXPREC                 40      // most steps of a revolved surface

struct FACE
{
    int         p[4];           // the 4 points of the quad
    int         material;
    D3DXVECTOR3 norm;
};

struct MESH
{
    int          numFaces;
    int          numPoints;
    D3DXVECTOR3* pts;
    D3DXVECTOR3* norms;         // one per point (smooth shading)
    FACE*        faces;
    int          listID;        // (the display list of the OpenGL version)
};

struct MATRIX
{
    FLOAT M[4][4];
};

// The vertices of the shared vertex buffers
#define D3DFVF_MESHVERTEX       (D3DFVF_XYZ | D3DFVF_NORMAL)
struct MESHVERTEX
{
    D3DXVECTOR3 p;
    D3DXVECTOR3 n;
};

#define D3DFVF_TEXVERTEX        (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)
struct TEXVERTEX
{
    D3DXVECTOR3 p;
    D3DXVECTOR3 n;
    FLOAT       tu, tv;
};

// Sizes of the shared buffers (RestoreDeviceObjects)
#define INDEX_BUFFER_SIZE       10000
#define VERTEX_BUFFER_SIZE      120000
#define TEXVERTEX_BUFFER_SIZE   160000

void  xformPoint( D3DXVECTOR3* ptOut, D3DXVECTOR3* ptIn, MATRIX* mat );
void  xformNorm( D3DXVECTOR3* ptOut, D3DXVECTOR3* ptIn, MATRIX* mat );
void  matrixIdent( MATRIX* mat );
void  matrixRotate( MATRIX* m, double xTheta, double yTheta, double zTheta );
void  matrixTranslate( MATRIX* m, double xTrans, double yTrans, double zTrans );
void  calcNorm( D3DXVECTOR3* norm, D3DXVECTOR3* p1, D3DXVECTOR3* p2, D3DXVECTOR3* p3 );
void  normalizeNorms( D3DXVECTOR3* p, ULONG cPts );
BOOL  newMesh( MESH* mesh, int numFaces, int numPts );
void  delMesh( MESH* mesh );
void  revolveSurface( MESH* triMesh, D3DXVECTOR3* curve, int steps );
void  updateObject( MESH* mesh, BOOL bSmooth );
void  updateObjectBack( MESH* mesh, BOOL bSmooth );
int   findElemExact( MESH* mesh, int iStart, D3DXVECTOR3* pt, BOOL bSearch, D3DXVECTOR3* norm );

extern D3DXVECTOR3 ptZero;


//-----------------------------------------------------------------------------
// The styles (one module each)
//-----------------------------------------------------------------------------
typedef BOOL (*PFNINITSCENE)();
typedef void (*PFNUPDATESCENE)( int flags, FLOAT fElapsedTime );
typedef void (*PFNDELSCENE)();

BOOL initWinLogoScene();
void updateWinLogoScene( int flags, FLOAT fElapsedTime );
void delWinLogoScene();

BOOL initExplodeScene();
void updateExplodeScene( int flags, FLOAT fElapsedTime );
void delExplodeScene();

BOOL initRibbonScene();
void updateRibbonScene( int flags, FLOAT fElapsedTime );
void delRibbonScene();

BOOL initSplashScene();
void updateSplashScene( int flags, FLOAT fElapsedTime );
void delSplashScene();

BOOL initTwistScene();
void updateTwistScene( int flags, FLOAT fElapsedTime );
void delTwistScene();

BOOL initFlagScene();
void updateFlagScene( int flags, FLOAT fElapsedTime );
void delFlagScene();


//-----------------------------------------------------------------------------
// Textures (texture.cpp)
//-----------------------------------------------------------------------------
struct TEXFILE
{
    int   nOffset;              // offset of the file name in szPathName
    TCHAR szPathName[MAX_PATH];
};

// The texture of the OpenGL version
struct TEXTURE
{
    int     width;
    int     height;
    int     format;
    int     components;
    FLOAT   origAspectRatio;
    BYTE*   data;
    UINT    texObj;
    int     pal_size;
    int     iPalRot;
    RGBQUAD* pal;
};

BOOL ss_LoadTextureResourceStrings();
void ss_DeleteTexture( TEXTURE* pTex );
void ss_GetDefaultBmpFile( LPTSTR pszBmpFile );
BOOL ss_SelectTextureFile( HWND hDlg, TEXFILE* ptf );


//-----------------------------------------------------------------------------
// Globals (ss3dfo.cpp)
//-----------------------------------------------------------------------------
class CFlyingObjectsScreensaver;
extern CFlyingObjectsScreensaver* g_pMyFlyingObjectsScreensaver;

extern LPDIRECT3DDEVICE8       g_pd3dDevice;   // the device being drawn
extern LPDIRECT3DINDEXBUFFER8  g_pIB;          // its shared buffers
extern LPDIRECT3DVERTEXBUFFER8 g_pVB;
extern LPDIRECT3DVERTEXBUFFER8 g_pVBTex;

extern BOOL     bSmoothShading;
extern BOOL     bColorCycle;
extern UINT     uSize;
extern FLOAT    fTesselFact;
extern int      Type;
extern TEXFILE  gTexFile;

extern D3DMATERIAL8 Material[];

struct FLOATRECT;
struct DeviceObjects;
extern FLOATRECT*       g_pFloatRect;       // the window of the object
extern POINT            g_ptMonitorOrigin;  // of the screen being drawn
extern DeviceObjects*   g_pDeviceObjects;   // of the device being drawn
extern BOOL             gbBounced;          // the window bounced
extern BOOL             gbReturnToOrigin;   // send the window back home
extern BOOL             gbAtOrigin;         // ... it is there
extern int              gulUpdateFlags;

void  HsvToRgb( FLOAT h, FLOAT s, FLOAT v, D3DXCOLOR* pColor );
void* SaverAlloc( ULONG size );
void  SaverFree( void* pMem );
void  myglMaterialfv( int face, int pname, FLOAT* params );
void  myglMaterialf( int face, int pname, FLOAT param );
void  SetProjectionParams( BOOL bOrtho, FLOAT fWidth, FLOAT fHeight, FLOAT fNear, FLOAT fFar );


//-----------------------------------------------------------------------------
// Name: struct FLOATRECT
// Desc: The window of the object, bouncing around the screen
//-----------------------------------------------------------------------------
struct FLOATRECT
{
    FLOAT xMin;
    FLOAT yMin;
    FLOAT xSize;
    FLOAT ySize;
    FLOAT xVel;
    FLOAT yVel;
    FLOAT xOrigin;              // where it started
    FLOAT yOrigin;
    FLOAT xVelMax;
    FLOAT yVelMax;
};


//-----------------------------------------------------------------------------
// Name: struct DeviceObjects
// Desc: The objects of a device
//-----------------------------------------------------------------------------
struct DeviceObjects
{
    LPDIRECT3DTEXTURE8      m_pTexture;
    LPDIRECT3DTEXTURE8      m_pTexture2;
    LPDIRECT3DINDEXBUFFER8  m_pIB;
    LPDIRECT3DVERTEXBUFFER8 m_pVB;
    LPDIRECT3DVERTEXBUFFER8 m_pVBTex;
};

#define MAX_DEVICE_OBJECTS  10


//-----------------------------------------------------------------------------
// Name: class CFlyingObjectsScreensaver
// Desc:
//-----------------------------------------------------------------------------
class CFlyingObjectsScreensaver : public CD3DScreensaver
{
protected:
    FLOATRECT       m_floatrect;
    DeviceObjects   m_DeviceObjectsArray[MAX_DEVICE_OBJECTS];
    DeviceObjects*  m_pDeviceObjects;

    // projection of the current style (SetProjectionParams)
    BOOL            m_bOrtho;
    FLOAT           m_fProjWidth;
    FLOAT           m_fProjHeight;
    FLOAT           m_fProjNear;
    FLOAT           m_fProjFar;

    friend void SetProjectionParams( BOOL bOrtho, FLOAT fWidth, FLOAT fHeight,
                                     FLOAT fNear, FLOAT fFar );

protected:
    virtual VOID    SetDevice( UINT iDevice );
    virtual HRESULT RegisterSoftwareDevice();
    virtual HRESULT ConfirmDevice( D3DCAPS8* pCaps, DWORD dwBehavior,
                                   D3DFORMAT fmtBackBuffer );
    virtual HRESULT RestoreDeviceObjects();
    virtual HRESULT InvalidateDeviceObjects();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();
    virtual VOID    DoConfig();
    virtual VOID    ReadSettings();

    BOOL    InitScene();
    VOID    WriteSettings( HWND hDlg );

    // Settings of the OpenGL version (control.ini)
    BOOL    ss_RegistrySetup( int section, int file );
    int     ss_GetRegistryInt( int name, int iDefault );
    VOID    ss_GetRegistryString( int name, LPTSTR lpDefault, LPTSTR lpDest, int bufSize );
    VOID    ss_ReadSettings();

    static INT_PTR CALLBACK ScreenSaverConfigureDialog( HWND hDlg, UINT message,
                                                        WPARAM wParam, LPARAM lParam );

public:
    CFlyingObjectsScreensaver();
};

#endif // _SS3DFO_H
