//-----------------------------------------------------------------------------
// File: pipes.h
//
// Desc: 3D Pipes screen saver (sspipes.scr) of Windows XP / Windows Server
//       2003 (x86), Direct3D 8 version: the screen saver class and the
//       settings.  The engine is in sspipes.h.
//
//       Original object:
//           d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\pipes.obj
//-----------------------------------------------------------------------------
#ifndef _PIPES_H
#define _PIPES_H

#define PIPES_MAX_DEVICES   10

//-----------------------------------------------------------------------------
// Settings (registry key Software\Microsoft\ScreenSavers\Pipes)
//-----------------------------------------------------------------------------
struct CONFIG
{
    BOOL    bMultiPipes;            // "MultiPipes": up to four pipes at a time
    int     nJointType;             // "Joint Type": JOINT_ELBOW .. JOINT_CYCLE
    BOOL    bTextured;              // "Textured"
    BOOL    bDefaultTexture;        // "Default Texture"
    // "Texture Name".  (sic: the dimensions are swapped, which works as only
    // the first texture is ever loaded: strTextureName[0] is the file name)
    TCHAR   strTextureName[MAX_PATH][MAX_TEXTURES];
    DWORD   dwSpeed;                // "Speed": 0 .. 20
    BOOL    bFlexMode;              // flexible pipes (not in the settings)
    DWORD   dwTesselFactor;         // "Tessel Factor": 0 .. 4
};

//-----------------------------------------------------------------------------
// Name: struct DeviceObjects
// Desc: Per-render-unit objects
//-----------------------------------------------------------------------------
struct DeviceObjects
{
    STATE*  m_pState;
};

//-----------------------------------------------------------------------------
// Name: class CPipesScreensaver
// Desc: D3D screensaver class
//-----------------------------------------------------------------------------
class CPipesScreensaver : public CD3DScreensaver
{
    friend class STATE;     // (reads the size of the back buffer)

protected:
    DeviceObjects   m_DeviceObjectsArray[PIPES_MAX_DEVICES];
    DeviceObjects*  m_pDeviceObjects;
    UINT            m_iDevice;
    DWORD           m_dwReserved[4];

public:
    CONFIG          m_config;

protected:
    virtual HRESULT RegisterSoftwareDevice();
    virtual HRESULT RestoreDeviceObjects();
    virtual HRESULT InvalidateDeviceObjects();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();
    virtual VOID    SetDevice( UINT iDevice );
    virtual VOID    DoPaint( HWND hwnd, HDC hdc );
    virtual VOID    DoConfig();
    virtual VOID    ReadSettings();

    VOID            WriteSettings();

public:
    CPipesScreensaver();

    BOOL            SelectTextureFile( HWND hDlg, TCHAR* strFile );
    BOOL            ss_RegistrySetup( int section, int file );
    int             ss_GetRegistryInt( int name, int iDefault );
    VOID            ss_GetRegistryString( int name, TCHAR* lpDefault, TCHAR* lpDest, int bufSize );
    VOID            ss_ReadSettings();

    INT_PTR WINAPI  ConfigureDialogProc( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );

    static int      iRand( int max );
    static int      iRand2( int min, int max );
    static FLOAT    fRand( FLOAT min, FLOAT max );
    static VOID     RandInit();
};

extern CPipesScreensaver* g_pMyPipesScreensaver;

#if defined(_M_IX86) || defined(__i386__)
static_assert( sizeof(CONFIG) == 0x105c, "CONFIG" );
static_assert( sizeof(CPipesScreensaver) == 0x3888, "CPipesScreensaver" );
#endif

#endif // _PIPES_H
