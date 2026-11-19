//-----------------------------------------------------------------------------
// File: flowerbox.h
//
// Desc: 3D FlowerBox screen saver (ssflwbox.scr) - the Direct3D 8 version
//       of the OpenGL screen saver of Windows NT, as shipped with Windows XP
//       and Windows Server 2003 (x86).
//
//       A cube, tetrahedron, pyramids, cylinder or spring is built out of
//       triangle strips; every point of the shape is pushed outwards or
//       pulled in along its direction from the centre ("bloom"), while the
//       shape spins and its window bounces around the screen.
//
//       Reconstructed from flowerbox.obj:
//
//           d:\nt\shell\osshell\control\scrnsave\flowerbox\obj\i386\flowerbox.obj
//
//       The names of all functions and globals are the ones of the symbol
//       file; the names of the members, structure fields and constants are
//       chosen by the reconstruction.
//-----------------------------------------------------------------------------
#ifndef _FLOWERBOX_H
#define _FLOWERBOX_H

#include "../d3dsaver/d3dsaver.h"
#include <d3dx8.h>

//-----------------------------------------------------------------------------
// Limits
//-----------------------------------------------------------------------------
#define MINSUBDIV       3
#define MAXSUBDIV       10
#define MAXSIDES        8                       // (pyramids)
#define NCCOLS          2                       // checkerboard colors per side

#define MAXPTS          (MAXSIDES*(MAXSUBDIV+1)*(MAXSUBDIV+1))  // 968
#define MAXSTRIPS       (MAXSIDES*MAXSUBDIV)                    // 80
#define MAXINDICES      (MAXPTS*2)                              // 1936

#define MINIMAGESIZE    10
#define MAXIMAGESIZE    100

// Size, in indices, of the two triangle lists DrawGeom() builds per side
#define MAX_TRI_INDICES 5000

// Values of CONFIG::two_sided.  The registry keeps the face constants of the
// OpenGL version of the screen saver.
#define GL_FRONT                0x0404          // backfaces culled
#define GL_FRONT_AND_BACK       0x0408          // both sides lit and drawn

// Shapes, indices into geom_table (CONFIG::geom)
enum { GEOM_CUBE, GEOM_TETRA, GEOM_PYRAMIDS, GEOM_CYLINDER, GEOM_SPRING, NGEOM };


//-----------------------------------------------------------------------------
// Structures
//-----------------------------------------------------------------------------

// Settings (registry key Software\Microsoft\Screensavers\Flowerbox)
struct CONFIG
{
    BOOL smooth_colors;     // "Smooth": Gouraud shading
    BOOL triangle_colors;   // "Slanted": checkerboard squares made of offset triangles
    BOOL cycle_colors;      // "Cycle": cycle the colors through the hues
    BOOL spin;              // "Spin"
    BOOL bloom;             // "Bloom"
    int  subdiv;            // "Subdiv": complexity, MINSUBDIV..MAXSUBDIV
    int  color_pick;        // "ColorPick": ID_COL_CHECKER, ID_COL_PER_SIDE or ID_COL_SINGLE
    int  image_size;        // "ImageSize": MINIMAGESIZE..MAXIMAGESIZE
    int  geom;              // "Geom": GEOM_*
    int  two_sided;         // "TwoSided": GL_FRONT or GL_FRONT_AND_BACK
};

// Range of a trackbar of the configuration dialog
struct _RANGE
{
    int min_val;
    int max_val;
    int step;
    int page_size;
};

// Maps the unit square onto one (planar) side of a shape:
//     point = base + x*x_axis + y*y_axis
struct PLANE_MAP
{
    D3DXVECTOR3 base;
    D3DXVECTOR3 x_axis;
    D3DXVECTOR3 y_axis;

    PLANE_MAP( D3DXVECTOR3 b, D3DXVECTOR3 x, D3DXVECTOR3 y )
    {
        base = b;
        x_axis = x;
        y_axis = y;
    }
};

// One side of a shape: a set of triangle strips
struct SIDE
{
    int   nstrips;          // Number of strips
    int*  strip_size;       // Number of vertices of each strip
    WORD* strip_index;      // Vertex indices of the strips, one after the other
};

// Vertex format of the shapes
#define D3DFVF_MYVERTEX (D3DFVF_XYZ | D3DFVF_NORMAL)

struct MYVERTEX
{
    D3DXVECTOR3 p;          // Position
    D3DXVECTOR3 n;          // Normal
};

struct GEOMETRY
{
    void (*init)( GEOMETRY* geom ); // Builds the shape for config.subdiv

    int  nsides;
    SIDE sides[MAXSIDES];

    D3DXVECTOR3* pts;       // Points of the shape
    D3DXVECTOR3* npts;      // Points scaled by the bloom factor
    D3DXVECTOR3* normals;   // Vertex normals (sums of the face normals)
    MYVERTEX*    vertices;  // Vertices handed to Direct3D
    int  total_pts;

    FLOAT min_sf;           // Range of the bloom (scale) factor
    FLOAT max_sf;
    FLOAT sf_inc;           // Bloom speed
    FLOAT init_sf;          // Scale of the shape (for the bloom lengths)
};


//-----------------------------------------------------------------------------
// Name: class CFlowerBoxScreensaver
// Desc: D3D screensaver class
//-----------------------------------------------------------------------------
class CFlowerBoxScreensaver : public CD3DScreensaver
{
protected:
    // Window of the shape, in screen coordinates; it bounces around the
    // screen
    FLOAT m_fViewportX;
    FLOAT m_fViewportY;
    FLOAT m_fViewportWidth;     // 0 until the first FrameMove()
    FLOAT m_fViewportHeight;
    FLOAT m_fViewportXVelocity;
    FLOAT m_fViewportYVelocity;

    FLOAT m_fXRot;              // Rotation, in degrees
    FLOAT m_fYRot;
    FLOAT m_fZRot;

    FLOAT m_fSf;                // Bloom (scale) factor
    FLOAT m_fSfInc;
    FLOAT m_fHueAngle;          // Hue of the first color, for cycling colors

    GEOMETRY* m_pGeom;          // Current shape

protected:
    virtual HRESULT RegisterSoftwareDevice();
    virtual HRESULT RestoreDeviceObjects();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();
    virtual VOID    DoConfig();
    virtual VOID    ReadSettings();

    VOID    InitVlen( GEOMETRY* geom, int npts, D3DXVECTOR3* pts );
    VOID    UpdatePts( GEOMETRY* geom, FLOAT sf );
    VOID    ComputeHsvColors();
    VOID    NewConfig( CONFIG* cnf );
    HRESULT SetMaterialColor( FLOAT* pfColors );
    VOID    DrawGeom( GEOMETRY* geom );

    // Settings of the OpenGL version (control.ini)
    BOOL    ss_RegistrySetup( int section, int file );
    int     ss_GetRegistryInt( int name, int iDefault );
    VOID    ss_ReadSettings();

    static INT_PTR CALLBACK ScreenSaverConfigureDialog( HWND hDlg, UINT message,
                                                        WPARAM wParam, LPARAM lParam );

public:
    CFlowerBoxScreensaver();
};

#if defined(_M_IX86) || defined(__i386__)
static_assert( sizeof(GEOMETRY) == 0x8c, "GEOMETRY" );
static_assert( sizeof(MYVERTEX) == 0x18, "MYVERTEX" );
static_assert( sizeof(CFlowerBoxScreensaver) == 0x2820, "CFlowerBoxScreensaver" );
#endif

#endif // _FLOWERBOX_H
