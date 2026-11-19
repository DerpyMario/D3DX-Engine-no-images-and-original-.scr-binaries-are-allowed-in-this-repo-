//-----------------------------------------------------------------------------
// File: text3d.h
//
// Desc: The 3D Text screen saver (sstext3d.scr) of Windows XP / Windows
//       Server 2003 (x86), Direct3D 8 version.
//
//       The class and its member functions are named as in the symbol file;
//       the names of the data members, of the enumerators and of the helper
//       structures are reconstructions (the screen saver descends from the
//       Text3D sample of the DirectX SDK, whose conventions are followed).
//       Its Windows Vista successor is shell\osshell\control\scrnsave\text3d
//       in this repository.
//
//       Original object:
//           d:\nt\shell\osshell\control\scrnsave\text3d\obj\i386\text3d.obj
//-----------------------------------------------------------------------------
#ifndef _TEXT3D_H
#define _TEXT3D_H

#include <windows.h>
#include <d3d8.h>
#include <d3dx8.h>
#include "../d3dsaver/d3dsaver.h"

#define MAX_DISPLAY_STRING      20
#define TEXT3D_MAX_DEVICES      10

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------
enum SurfType
{
    color = 0,          // solid color
    environment = 1,    // sphere mapped reflection
    texture = 2         // texture projected through the text
};

enum RotType
{
    none = 0,
    spin = 1,
    seesaw = 2,
    wobble = 3,
    tumble = 4
};

//-----------------------------------------------------------------------------
// Name: struct DeviceObjects
// Desc: Per-render-unit objects
//-----------------------------------------------------------------------------
struct DeviceObjects
{
    ID3DXMesh*          m_pMesh;
    IDirect3DTexture8*  m_pTexture;
    DWORD               m_dwMeshUpdateCounter;  // the mesh is rebuilt when this lags behind
};

//-----------------------------------------------------------------------------
// Name: struct FLOATRECT
// Desc: The square the text is rendered into, in virtual screen coordinates
//-----------------------------------------------------------------------------
struct FLOATRECT
{
    FLOAT xMin;
    FLOAT yMin;
    FLOAT xSize;
    FLOAT ySize;
    FLOAT xVel;
    FLOAT yVel;
};

//-----------------------------------------------------------------------------
// Name: class CTextScreensaver
//-----------------------------------------------------------------------------
class CTextScreensaver : public CD3DScreensaver
{
public:
                    CTextScreensaver();

    virtual VOID    DoConfig();

protected:
    virtual HRESULT RegisterSoftwareDevice();
    virtual HRESULT ConfirmDevice( D3DCAPS8* pCaps, DWORD dwBehavior, D3DFORMAT fmtBackBuffer );
    virtual HRESULT FrameMove();
    virtual HRESULT Render();
    virtual HRESULT RestoreDeviceObjects();
    virtual HRESULT InvalidateDeviceObjects();
    virtual VOID    ReadSettings();
    virtual VOID    SetDevice( UINT iDevice );

    VOID            SetPerFrameStates();
    IDirect3DTexture8* CreateTextureFromFile( const TCHAR* pszFile );
    HRESULT         BuildTextMesh( const TCHAR* pszDisplayString );
    VOID            UpdateAngles( DWORD dwElapsed );
    BOOL            UpdateTimeString( TCHAR* pszTime );

    // the settings of the OpenGL version of this screen saver
    BOOL            ss_RegistrySetup( int section, int file );
    int             ss_GetRegistryInt( int name, int iDefault );
    VOID            ss_GetRegistryString( int name, TCHAR* lpDefault, TCHAR* lpDest, int bufSize );
    VOID            ss_ReadSettings();

    // settings dialog
    static INT_PTR CALLBACK SettingsDialogProcStub( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
    BOOL            SettingsDialogProc( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
    VOID            InitItems( HWND hDlg );
    VOID            ExtractAndWriteSettings( HWND hDlg );
    VOID            SelectSurfaceColor( HWND hDlg );
    VOID            SelectFont( HWND hDlg );
    VOID            SelectCustomTexture( HWND hDlg );
    VOID            SelectCustomEnvironment( HWND hDlg );
    VOID            EnableTextureWindows( HWND hDlg, SurfType surfType );

protected:
    DeviceObjects   m_DeviceObjectsArray[TEXT3D_MAX_DEVICES];
    DeviceObjects*  m_pDeviceObjects;

    FLOATRECT       m_floatrect;
    HFONT           m_hFont;

    // extent of the text mesh, and what it takes to center it
    FLOAT           m_fTextMinX;
    FLOAT           m_fTextMaxX;
    FLOAT           m_fTextMinY;
    FLOAT           m_fTextMaxY;
    FLOAT           m_fTextOffsetX;
    FLOAT           m_fTextOffsetY;

    FLOAT           m_fAngleX;
    FLOAT           m_fAngleY;
    FLOAT           m_fAngleZ;

    DWORD           m_dwLastTick;
    DWORD           m_dwMeshUpdateCounter;

    // settings
    TCHAR           m_szDisplayString[MAX_DISPLAY_STRING + 1];
    LOGFONT         m_Font;
    BOOL            m_bUseTime;                 // show the time instead of the string
    SurfType        m_SurfType;
    BOOL            m_bSpecular;
    DWORD           m_dwRotationSpeed;          // 1 .. 20
    DWORD           m_dwSize;                   // 1 .. 10
    RotType         m_RotType;
    COLORREF        m_SurfaceColor;
    BOOL            m_bUseCustomColor;
    BOOL            m_bUseCustomTexture;
    BOOL            m_bUseCustomEnvironment;
    TCHAR           m_szCustomTexture[MAX_PATH];
    TCHAR           m_szCustomEnvironment[MAX_PATH];
    DWORD           m_dwMeshQuality;            // 0 .. 1000

    D3DXMATRIX      m_matWorld;
    D3DXMATRIX      m_matView;
};

#if defined(_M_IX86) || defined(__i386__)
static_assert( sizeof(DeviceObjects) == 0xc, "DeviceObjects layout" );
static_assert( sizeof(CTextScreensaver) == 0x2df4, "CTextScreensaver layout" );
#endif

#endif // _TEXT3D_H
