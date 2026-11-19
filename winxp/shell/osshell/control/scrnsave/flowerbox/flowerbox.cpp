//-----------------------------------------------------------------------------
// File: flowerbox.cpp
//
// Desc: 3D FlowerBox screen saver (ssflwbox.scr), Direct3D 8 version.
//
//       Reconstructed from flowerbox.obj of the Windows XP / Windows Server
//       2003 (x86) build:
//
//           d:\nt\shell\osshell\control\scrnsave\flowerbox\obj\i386\flowerbox.obj
//
//       Each side of a shape is a grid of points (MapToSide maps the unit
//       square or triangle onto it) connected by triangle strips.  "Bloom"
//       scales every point by 1 + sf*vlen, where vlen is chosen so that a
//       scale factor of 1 would put all points on a sphere: the shape
//       swells into a ball and folds back inside out.
//-----------------------------------------------------------------------------
#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include <string.h>
#include "flowerbox.h"
#include "../d3dsaver/dxutil.h"
#include "resource.h"

#ifndef NO_D3D8RGB
// Software rasterizer of the DirectX 8 DDK, linked from d3d8rgb.lib (a
// library internal to the Windows source tree: public\internal\multimedia).
extern "C" void* D3D8RGBRasterizer;
#endif

#define PI      D3DX_PI

#ifndef min     // (windef.h of the Windows SDK)
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
CFlowerBoxScreensaver* g_pMyFlowerBoxScreensaver = NULL;

// Base colors: two per side for the checkerboard, one per side, and the
// single color
FLOAT base_checker_cols[MAXSIDES][NCCOLS][4] =
{
    { {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f} },
    { {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f} },
    { {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f} },
    { {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f} },
    { {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f} },
    { {1.0f, 1.0f, 0.0f, 1.0f}, {0.5f, 0.5f, 1.0f, 1.0f} },
    { {0.5f, 0.5f, 1.0f, 1.0f}, {1.0f, 0.5f, 0.5f, 1.0f} },
    { {1.0f, 0.5f, 0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} },
};
FLOAT checker_cols[MAXSIDES][NCCOLS][4];

FLOAT base_side_cols[MAXSIDES][4] =
{
    {1.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 1.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 1.0f, 1.0f},
    {1.0f, 0.0f, 1.0f, 1.0f},
    {0.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 0.0f, 1.0f},
    {0.5f, 0.5f, 1.0f, 1.0f},
    {1.0f, 0.5f, 0.5f, 1.0f},
};
FLOAT side_cols[MAXSIDES][4];

FLOAT base_solid_cols[4] =
{
    1.0f, 1.0f, 1.0f, 1.0f
};
FLOAT solid_cols[4];

// Current settings and the defaults
CONFIG config =
{
    TRUE,               // smooth_colors
    FALSE,              // triangle_colors
    FALSE,              // cycle_colors
    TRUE,               // spin
    TRUE,               // bloom
    MAXSUBDIV,          // subdiv
    ID_COL_PER_SIDE,    // color_pick
    (MINIMAGESIZE+MAXIMAGESIZE)/2,  // image_size
    GEOM_CUBE,          // geom
    GL_FRONT            // two_sided
};

// Settings being edited in the configuration dialog
CONFIG temp_config;

_RANGE complexity_range = { MINSUBDIV, MAXSUBDIV, 1, 2 };
_RANGE image_size_range = { MINIMAGESIZE, MAXIMAGESIZE, 1, 10 };

// Storage shared by all the shapes (only one is built at a time)
D3DXVECTOR3 pts[MAXPTS];
D3DXVECTOR3 npts[MAXPTS];
D3DXVECTOR3 normals[MAXPTS];
MYVERTEX    vertices[MAXPTS];
FLOAT       vlen[MAXPTS];
int         strip_size[MAXSTRIPS];
WORD        index[MAXINDICES];

void InitCube( GEOMETRY* geom );
void InitTetra( GEOMETRY* geom );
void InitPyramids( GEOMETRY* geom );
void InitCylinder( GEOMETRY* geom );
void InitSpring( GEOMETRY* geom );

GEOMETRY cube_geom     = { InitCube };
GEOMETRY tetra_geom    = { InitTetra };
GEOMETRY pyramids_geom = { InitPyramids };
GEOMETRY cylinder_geom = { InitCylinder };
GEOMETRY spring_geom   = { InitSpring };

GEOMETRY* geom_table[NGEOM] =
{
    &cube_geom,
    &tetra_geom,
    &pyramids_geom,
    &cylinder_geom,
    &spring_geom
};

// control.ini section and file name, for the settings of the OpenGL version
TCHAR g_szSectName[256];
TCHAR g_szFname[256];


//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow )
{
    HRESULT hr;
    CFlowerBoxScreensaver flowerboxSS;

    if( FAILED( hr = flowerboxSS.Create( hInst ) ) )
    {
        flowerboxSS.DisplayErrorMsg( hr );
        return 0;
    }

    return flowerboxSS.Run();
}




//-----------------------------------------------------------------------------
// Name: CFlowerBoxScreensaver()
// Desc: Constructor
//-----------------------------------------------------------------------------
CFlowerBoxScreensaver::CFlowerBoxScreensaver()
{
    g_pMyFlowerBoxScreensaver = this;

    LoadString( NULL, IDS_DESCRIPTION, m_strWindowTitle, 200 );
    m_bUseDepthBuffer = TRUE;

    lstrcpy( m_strRegPath, TEXT("Software\\Microsoft\\Screensavers\\Flowerbox") );

    m_fXRot = 0.0f;
    m_pGeom = NULL;
    m_fYRot = 0.0f;
    m_fZRot = 0.0f;
    m_fSf = 0.0f;
    m_fSfInc = 0.0f;
    m_fHueAngle = 0.0f;
    m_fViewportWidth = 0.0f;

    InitCommonControls();
}




//-----------------------------------------------------------------------------
// Name: RegisterSoftwareDevice()
// Desc: This can register the D3D8RGBRasterizer or any other
//       pluggable software rasterizer.
//-----------------------------------------------------------------------------
HRESULT CFlowerBoxScreensaver::RegisterSoftwareDevice()
{
#ifndef NO_D3D8RGB
    m_pD3D->RegisterSoftwareDevice( D3D8RGBRasterizer );
#endif

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InitVlen()
// Desc: For each point, the factor that would move it onto the sphere of
//       radius 1/init_sf.
//-----------------------------------------------------------------------------
VOID CFlowerBoxScreensaver::InitVlen( GEOMETRY* geom, int npts, D3DXVECTOR3* pts )
{
    FLOAT d;
    FLOAT* vl = vlen;

    while( npts-- > 0 )
    {
        d = D3DXVec3Length( pts ) * geom->init_sf;

        // Don't allow really close points because this leads to
        // numeric instability and really large objects
        *vl++ = (1 - d) / d;

        pts++;
    }
}




//-----------------------------------------------------------------------------
// Name: MapToSide()
// Desc: Maps the point (x, y) of the unit square onto one side of a shape
//-----------------------------------------------------------------------------
void MapToSide( PLANE_MAP* map, FLOAT x, FLOAT y, D3DXVECTOR3* pt )
{
    pt->x = x*map->x_axis.x + y*map->y_axis.x + map->base.x;
    pt->y = x*map->x_axis.y + y*map->y_axis.y + map->base.y;
    pt->z = x*map->x_axis.z + y*map->y_axis.z + map->base.z;
}




//-----------------------------------------------------------------------------
// Name: InitCube()
// Desc: Six square sides, each a grid of (subdiv+1)^2 points: one strip per
//       row of squares.
//-----------------------------------------------------------------------------
void InitCube( GEOMETRY* geom )
{
    PLANE_MAP cube_planes[6] =
    {
        PLANE_MAP( D3DXVECTOR3(-0.5f, -0.5f,  0.5f), D3DXVECTOR3( 1.0f,  0.0f, 0.0f), D3DXVECTOR3(0.0f, 1.0f, 0.0f) ),
        PLANE_MAP( D3DXVECTOR3( 0.5f, -0.5f, -0.5f), D3DXVECTOR3(-1.0f,  0.0f, 0.0f), D3DXVECTOR3(0.0f, 1.0f, 0.0f) ),
        PLANE_MAP( D3DXVECTOR3( 0.5f,  0.5f, -0.5f), D3DXVECTOR3(-1.0f,  0.0f, 0.0f), D3DXVECTOR3(0.0f, 0.0f, 1.0f) ),
        PLANE_MAP( D3DXVECTOR3(-0.5f, -0.5f, -0.5f), D3DXVECTOR3( 1.0f,  0.0f, 0.0f), D3DXVECTOR3(0.0f, 0.0f, 1.0f) ),
        PLANE_MAP( D3DXVECTOR3( 0.5f, -0.5f, -0.5f), D3DXVECTOR3( 0.0f,  1.0f, 0.0f), D3DXVECTOR3(0.0f, 0.0f, 1.0f) ),
        PLANE_MAP( D3DXVECTOR3(-0.5f,  0.5f, -0.5f), D3DXVECTOR3( 0.0f, -1.0f, 0.0f), D3DXVECTOR3(0.0f, 0.0f, 1.0f) ),
    };
    int side, x, y;
    int side_pts;
    int* sz;
    WORD* idx;
    D3DXVECTOR3* pt;

    side_pts = (config.subdiv+1)*(config.subdiv+1);

    geom->nsides = 6;
    geom->pts = pts;
    geom->npts = npts;
    geom->normals = normals;
    geom->vertices = vertices;

    geom->min_sf = -1.1f;
    geom->max_sf = 5.1f;
    geom->sf_inc = 0.05f;
    geom->init_sf = 2.0f;

    // Build the triangle strips: a strip of (subdiv+1)*2 vertices for each
    // row of squares
    sz = strip_size;
    idx = index;
    for( side = 0; side < geom->nsides; side++ )
    {
        geom->sides[side].nstrips = config.subdiv;
        geom->sides[side].strip_size = sz;
        geom->sides[side].strip_index = idx;

        for( x = 0; x < config.subdiv; x++ )
        {
            *sz++ = (config.subdiv+1)*2;
        }

        for( x = 0; x < config.subdiv; x++ )
        {
            for( y = 0; y < config.subdiv+1; y++ )
            {
                *idx++ = (WORD)( side*side_pts + x*(config.subdiv+1) + y );
                *idx++ = (WORD)( side*side_pts + (x+1)*(config.subdiv+1) + y );
            }
        }
    }

    // Generate the points
    pt = geom->pts;
    for( side = 0; side < geom->nsides; side++ )
    {
        for( x = 0; x < config.subdiv+1; x++ )
        {
            for( y = 0; y < config.subdiv+1; y++ )
            {
                MapToSide( &cube_planes[side], (FLOAT)x/(FLOAT)config.subdiv,
                           (FLOAT)y/(FLOAT)config.subdiv, pt );
                pt++;
            }
        }
    }

    geom->total_pts = geom->nsides*side_pts;
}




// Cosine of 30 degrees, as written in the source (sqrt(3)/2 = 0.8660254...)
#define SQRT3_2         0.866025f

//-----------------------------------------------------------------------------
// Name: InitTetra()
// Desc: Four triangular sides, each a triangle of (subdiv+1)(subdiv+2)/2
//       points: one strip per row of triangles.
//-----------------------------------------------------------------------------
void InitTetra( GEOMETRY* geom )
{
    PLANE_MAP tetra_planes[4] =
    {
        PLANE_MAP( D3DXVECTOR3(-0.5f, -SQRT3_2/4, SQRT3_2/3),
                   D3DXVECTOR3( 1.0f,  0.0f, 0.0f),
                   D3DXVECTOR3( 0.0f,  SQRT3_2, -SQRT3_2/3) ),
        PLANE_MAP( D3DXVECTOR3( 0.0f, -SQRT3_2/4, -SQRT3_2*2/3),
                   D3DXVECTOR3(-0.5f,  0.0f, SQRT3_2),
                   D3DXVECTOR3( 0.25f, SQRT3_2, SQRT3_2/6) ),
        PLANE_MAP( D3DXVECTOR3( 0.5f, -SQRT3_2/4, SQRT3_2/3),
                   D3DXVECTOR3(-0.5f,  0.0f, -SQRT3_2),
                   D3DXVECTOR3(-0.25f, SQRT3_2, SQRT3_2/6) ),
        PLANE_MAP( D3DXVECTOR3( 0.5f, -SQRT3_2/4, SQRT3_2/3),
                   D3DXVECTOR3(-1.0f,  0.0f, 0.0f),
                   D3DXVECTOR3( 0.0f,  0.0f, -SQRT3_2) ),
    };
    int side, x, y;
    WORD side_pts, row_pts, base_pt;
    int* sz;
    WORD* idx;
    D3DXVECTOR3* pt;
    FLOAT fx;

    side_pts = (config.subdiv+2)*(config.subdiv+1)/2;

    geom->nsides = 4;
    geom->pts = pts;
    geom->npts = npts;
    geom->normals = normals;
    geom->vertices = vertices;

    geom->min_sf = -1.1f;
    geom->max_sf = 5.2f;
    geom->sf_inc = 0.05f;
    geom->init_sf = 3.75f;

    // Build the triangle strips: row x has subdiv-x+1 points, and the strip
    // between it and the next row (subdiv-x+1)*2-1 vertices
    base_pt = 0;
    sz = strip_size;
    idx = index;
    for( side = 0; side < geom->nsides; side++ )
    {
        geom->sides[side].nstrips = config.subdiv;
        geom->sides[side].strip_size = sz;
        geom->sides[side].strip_index = idx;

        for( x = 0; x < config.subdiv; x++ )
        {
            row_pts = config.subdiv-x+1;
            *sz++ = row_pts*2-1;

            *idx++ = base_pt;
            for( y = 0; y < row_pts-1; y++ )
            {
                *idx++ = base_pt+row_pts+y;
                *idx++ = base_pt+y+1;
            }
            base_pt += row_pts;
        }

        // (the last row is the tip of the triangle)
        base_pt++;
    }

    // Generate the points
    pt = geom->pts;
    for( side = 0; side < geom->nsides; side++ )
    {
        for( x = 0; x < config.subdiv+1; x++ )
        {
            fx = (FLOAT)x/(FLOAT)config.subdiv;
            for( y = 0; y < config.subdiv-x+1; y++ )
            {
                MapToSide( &tetra_planes[side],
                           (FLOAT)y/(FLOAT)(config.subdiv*2) + fx,
                           (FLOAT)y/(FLOAT)config.subdiv, pt );
                pt++;
            }
        }
    }

    geom->total_pts = geom->nsides*side_pts;
}




//-----------------------------------------------------------------------------
// Name: InitPyramids()
// Desc: Two square pyramids base to base: eight triangular sides, built as
//       the sides of the tetrahedron.
//-----------------------------------------------------------------------------
void InitPyramids( GEOMETRY* geom )
{
    PLANE_MAP pyramids_planes[8] =
    {
        PLANE_MAP( D3DXVECTOR3(-0.5f, 0.0f,  0.5f), D3DXVECTOR3( 1.0f, 0.0f,  0.0f), D3DXVECTOR3( 0.0f,  0.5f, -0.5f) ),
        PLANE_MAP( D3DXVECTOR3( 0.5f, 0.0f,  0.5f), D3DXVECTOR3(-1.0f, 0.0f,  0.0f), D3DXVECTOR3( 0.0f, -0.5f, -0.5f) ),
        PLANE_MAP( D3DXVECTOR3( 0.5f, 0.0f,  0.5f), D3DXVECTOR3( 0.0f, 0.0f, -1.0f), D3DXVECTOR3(-0.5f,  0.5f,  0.0f) ),
        PLANE_MAP( D3DXVECTOR3( 0.5f, 0.0f, -0.5f), D3DXVECTOR3( 0.0f, 0.0f,  1.0f), D3DXVECTOR3(-0.5f, -0.5f,  0.0f) ),
        PLANE_MAP( D3DXVECTOR3( 0.5f, 0.0f, -0.5f), D3DXVECTOR3(-1.0f, 0.0f,  0.0f), D3DXVECTOR3( 0.0f,  0.5f,  0.5f) ),
        PLANE_MAP( D3DXVECTOR3(-0.5f, 0.0f, -0.5f), D3DXVECTOR3( 1.0f, 0.0f,  0.0f), D3DXVECTOR3( 0.0f, -0.5f,  0.5f) ),
        PLANE_MAP( D3DXVECTOR3(-0.5f, 0.0f, -0.5f), D3DXVECTOR3( 0.0f, 0.0f,  1.0f), D3DXVECTOR3( 0.5f,  0.5f,  0.0f) ),
        PLANE_MAP( D3DXVECTOR3(-0.5f, 0.0f,  0.5f), D3DXVECTOR3( 0.0f, 0.0f, -1.0f), D3DXVECTOR3( 0.5f, -0.5f,  0.0f) ),
    };
    int side, x, y;
    WORD side_pts, row_pts, base_pt;
    int* sz;
    WORD* idx;
    D3DXVECTOR3* pt;
    FLOAT fx;

    side_pts = (config.subdiv+2)*(config.subdiv+1)/2;

    geom->nsides = 8;
    geom->pts = pts;
    geom->npts = npts;
    geom->normals = normals;
    geom->vertices = vertices;

    geom->min_sf = -1.1f;
    geom->max_sf = 5.2f;
    geom->sf_inc = 0.05f;
    geom->init_sf = 3.0f;

    // Build the triangle strips
    base_pt = 0;
    sz = strip_size;
    idx = index;
    for( side = 0; side < geom->nsides; side++ )
    {
        geom->sides[side].nstrips = config.subdiv;
        geom->sides[side].strip_size = sz;
        geom->sides[side].strip_index = idx;

        for( x = 0; x < config.subdiv; x++ )
        {
            row_pts = config.subdiv-x+1;
            *sz++ = row_pts*2-1;

            *idx++ = base_pt;
            for( y = 0; y < row_pts-1; y++ )
            {
                *idx++ = base_pt+row_pts+y;
                *idx++ = base_pt+y+1;
            }
            base_pt += row_pts;
        }

        base_pt++;
    }

    // Generate the points
    pt = geom->pts;
    for( side = 0; side < geom->nsides; side++ )
    {
        for( x = 0; x < config.subdiv+1; x++ )
        {
            fx = (FLOAT)x/(FLOAT)config.subdiv;
            for( y = 0; y < config.subdiv-x+1; y++ )
            {
                MapToSide( &pyramids_planes[side],
                           (FLOAT)y/(FLOAT)(config.subdiv*2) + fx,
                           (FLOAT)y/(FLOAT)config.subdiv, pt );
                pt++;
            }
        }
    }

    geom->total_pts = geom->nsides*side_pts;
}




//-----------------------------------------------------------------------------
// Name: InitCylinder()
// Desc: An open cylinder: subdiv columns of subdiv+1 points around the axis,
//       one strip per column (the last one wraps around to the first).
//-----------------------------------------------------------------------------
void InitCylinder( GEOMETRY* geom )
{
    WORD side, x, y;
    WORD row_pts;
    int base_pt;
    int* sz;
    WORD* idx;
    D3DXVECTOR3* pt;
    double ang, fx, fz;

    geom->nsides = 1;
    geom->pts = pts;
    geom->npts = npts;
    geom->normals = normals;
    geom->vertices = vertices;

    geom->min_sf = -2.5f;
    geom->max_sf = 8.5f;
    geom->sf_inc = 0.05f;
    geom->init_sf = 2.1f;

    // Build the triangle strips
    side = 0;
    geom->sides[side].nstrips = config.subdiv;
    geom->sides[side].strip_size = strip_size;
    geom->sides[side].strip_index = index;

    sz = strip_size;
    idx = index;
    row_pts = config.subdiv+1;
    base_pt = 0;
    for( x = 0; x < config.subdiv; x++ )
    {
        *sz++ = row_pts*2;

        for( y = 0; y < row_pts; y++ )
        {
            // Wrap around at the end
            if( x == config.subdiv-1 )
            {
                *idx++ = y;
            }
            else
            {
                *idx++ = base_pt+row_pts+y;
            }
            *idx++ = base_pt+y;
        }

        base_pt += row_pts;
    }

    // Generate the points
    ang = 0;
    pt = geom->pts;
    for( x = 0; x < config.subdiv; x++ )
    {
        fx = cos(ang)*0.5f;
        fz = sin(ang)*0.5f;
        for( y = 0; y < config.subdiv+1; y++ )
        {
            pt->x = (FLOAT)fx;
            pt->y = (FLOAT)y/config.subdiv-0.5f;
            pt->z = (FLOAT)fz;
            pt++;
        }
        ang += (2*PI)/config.subdiv;
    }

    geom->total_pts = geom->nsides*(config.subdiv+1)*config.subdiv;
}




//-----------------------------------------------------------------------------
// Name: InitSpring()
// Desc: Two turns of a coil: a ring of subdiv points around the wire for
//       each of the subdiv*4+1 steps along it; one strip per ring segment.
//-----------------------------------------------------------------------------
void InitSpring( GEOMETRY* geom )
{
    int side, x, y;
    int rings, ring_pts;
    int* sz;
    WORD* idx;
    D3DXVECTOR3* pt;
    double ang, rang;
    FLOAT fRings, dang;
    PLANE_MAP plane( D3DXVECTOR3(0, 0, 0), D3DXVECTOR3(0, 0, 0), D3DXVECTOR3(0, 0, 0) );

    geom->init_sf = 1.0f;
    geom->nsides = 1;
    geom->pts = pts;
    geom->npts = npts;
    geom->normals = normals;
    geom->vertices = vertices;

    geom->min_sf = -2.2f;
    geom->max_sf = 0.2f;
    geom->sf_inc = 0.05f;

    // Build the triangle strips
    side = 0;
    geom->sides[side].nstrips = config.subdiv;
    geom->sides[side].strip_size = strip_size;
    geom->sides[side].strip_index = index;

    ring_pts = config.subdiv;
    rings = config.subdiv*4+1;

    sz = strip_size;
    for( x = 0; x < ring_pts; x++ )
    {
        *sz++ = rings*2;
    }

    idx = index;
    for( x = 0; x < ring_pts; x++ )
    {
        for( y = 0; y < rings; y++ )
        {
            *idx++ = (WORD)( y*ring_pts+x );
            // Wrap around at the end
            if( x == ring_pts-1 )
            {
                *idx++ = (WORD)( y*ring_pts );
            }
            else
            {
                *idx++ = (WORD)( y*ring_pts+x+1 );
            }
        }
    }

    // Generate the points: each ring lies in the plane through the axis of
    // the coil, centred on the wire, which rises from y = -0.5 to y = 0.5
    // while its distance from the axis shrinks from 0.5 to 0.3
    plane.x_axis.y = 0.0f;
    plane.y_axis.x = 0.0f;
    plane.y_axis.y = 0.1f;
    plane.y_axis.z = 0.0f;

    ang = 0;
    pt = geom->pts;
    if( rings > 0 )
    {
        fRings = (FLOAT)(rings-1);
        dang = (4*PI)/fRings;
        for( x = 0; x < rings; x++ )
        {
            double c = cos(ang);
            double s = sin(ang);
            double r = 0.5f - 0.2f*(x/fRings);

            plane.base.x = (FLOAT)(r*c);
            plane.base.y = (FLOAT)(x/fRings - 0.5f);
            plane.base.z = (FLOAT)(r*s);
            plane.x_axis.x = (FLOAT)(c*0.1f);
            plane.x_axis.z = (FLOAT)(s*0.1f);

            rang = 0;
            for( y = 0; y < config.subdiv; y++ )
            {
                MapToSide( &plane, (FLOAT)cos(rang), (FLOAT)sin(rang), pt );
                pt++;
                rang += (2*PI)/config.subdiv;
            }

            ang += dang;
        }
    }

    geom->total_pts = geom->nsides*ring_pts*rings;
}




//-----------------------------------------------------------------------------
// Name: ComputeAveragedNormals()
// Desc: Sums the normals of the triangles around each point.  The vertex
//       normals are normalized when they are copied to the vertices.
//-----------------------------------------------------------------------------
void ComputeAveragedNormals( GEOMETRY* geom )
{
    int side, strip, vt;
    int* sz;
    WORD* idx;
    WORD v0, v1, v2;
    D3DXVECTOR3 n;

    memset( geom->normals, 0, sizeof(D3DXVECTOR3)*geom->total_pts );

    for( side = 0; side < geom->nsides; side++ )
    {
        idx = geom->sides[side].strip_index;
        sz = geom->sides[side].strip_size;
        for( strip = 0; strip < geom->sides[side].nstrips; strip++ )
        {
            v0 = *idx++;
            v1 = *idx++;
            for( vt = 0; vt < *sz-2; vt++ )
            {
                v2 = *idx++;

                D3DXVECTOR3 e1 = geom->npts[v2] - geom->npts[v0];
                D3DXVECTOR3 e2 = geom->npts[v1] - geom->npts[v0];
                D3DXVec3Cross( &n, &e1, &e2 );

                // Every other triangle of a strip is wound the other way
                if( (vt & 1) == 0 )
                {
                    n.x = -n.x;
                    n.y = -n.y;
                    n.z = -n.z;
                }

                geom->normals[v0] += n;
                geom->normals[v1] += n;
                geom->normals[v2] += n;

                v0 = v1;
                v1 = v2;
            }
            sz++;
        }
    }
}




//-----------------------------------------------------------------------------
// Name: UpdatePts()
// Desc: Scales the points by the bloom factor sf and recomputes the normals.
//-----------------------------------------------------------------------------
VOID CFlowerBoxScreensaver::UpdatePts( GEOMETRY* geom, FLOAT sf )
{
    int pt;
    FLOAT f;
    FLOAT* vl;
    D3DXVECTOR3* v;
    D3DXVECTOR3* p;

    vl = vlen;
    v = geom->pts;
    p = geom->npts;
    for( pt = 0; pt < geom->total_pts; pt++ )
    {
        f = sf * (*vl++) + 1.0f;
        p->x = f * v->x;
        p->y = f * v->y;
        p->z = f * v->z;
        p++;
        v++;
    }

    ComputeAveragedNormals( geom );
}




//-----------------------------------------------------------------------------
// Name: ComputeHsvColors()
// Desc: Spreads the colors evenly around the hue circle, starting at
//       m_fHueAngle (fully saturated, full value).
//-----------------------------------------------------------------------------
VOID CFlowerBoxScreensaver::ComputeHsvColors()
{
    FLOAT* cols;
    int ncols;
    FLOAT ang, da;
    int hex;
    FLOAT fhex, frac;
    FLOAT p, q, t;

    switch( config.color_pick )
    {
    case ID_COL_CHECKER:
        ncols = MAXSIDES*NCCOLS;
        cols = &checker_cols[0][0][0];
        break;
    case ID_COL_PER_SIDE:
        ncols = MAXSIDES;
        cols = &side_cols[0][0];
        break;
    case ID_COL_SINGLE:
        ncols = 1;
        cols = &solid_cols[0];
        break;
    default:
        // (left uninitialized by the original)
        ncols = 0;
        cols = NULL;
        break;
    }

    ang = m_fHueAngle;
    da = (2*PI)/ncols;
    for( int i = 0; i < ncols; i++ )
    {
        fhex = ang * (6/(2*PI));
        hex = (int)fhex;
        frac = fhex-hex;
        hex = hex % 6;

        // Saturation and value are 1
        p = 0;
        q = 1-frac;
        t = 1.0f-(1-frac);

        switch( hex )
        {
        case 0:
            cols[0] = 1.0f;
            cols[1] = t;
            cols[2] = p;
            break;
        case 1:
            cols[0] = q;
            cols[1] = 1.0f;
            cols[2] = p;
            break;
        case 2:
            cols[0] = p;
            cols[1] = 1.0f;
            cols[2] = t;
            break;
        case 3:
            cols[0] = p;
            cols[1] = q;
            cols[2] = 1.0f;
            break;
        case 4:
            cols[0] = t;
            cols[1] = p;
            cols[2] = 1.0f;
            break;
        case 5:
            cols[0] = 1.0f;
            cols[1] = p;
            cols[2] = q;
            break;
        }

        ang += da;
        cols += 4;
    }
}




//-----------------------------------------------------------------------------
// Name: NewConfig()
// Desc: Applies (and saves) a set of settings: rebuilds the current shape.
//-----------------------------------------------------------------------------
VOID CFlowerBoxScreensaver::NewConfig( CONFIG* cnf )
{
    HKEY hkey;

    config = *cnf;

    // Save the settings
    if( ERROR_SUCCESS == RegCreateKeyEx( HKEY_CURRENT_USER, m_strRegPath,
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL ) )
    {
        DXUtil_WriteBoolRegKey( hkey, TEXT("Smooth"), config.smooth_colors );
        DXUtil_WriteBoolRegKey( hkey, TEXT("Slanted"), config.triangle_colors );
        DXUtil_WriteBoolRegKey( hkey, TEXT("Cycle"), config.cycle_colors );
        DXUtil_WriteBoolRegKey( hkey, TEXT("Spin"), config.spin );
        DXUtil_WriteBoolRegKey( hkey, TEXT("Bloom"), config.bloom );
        DXUtil_WriteIntRegKey( hkey, TEXT("Subdiv"), config.subdiv );
        DXUtil_WriteIntRegKey( hkey, TEXT("ColorPick"), config.color_pick );
        DXUtil_WriteIntRegKey( hkey, TEXT("ImageSize"), config.image_size );
        DXUtil_WriteIntRegKey( hkey, TEXT("Geom"), config.geom );
        DXUtil_WriteIntRegKey( hkey, TEXT("TwoSided"), config.two_sided );

        WriteScreenSettings( hkey );

        RegCloseKey( hkey );
    }

    // Reset the colors (they may have been cycled)
    memcpy( checker_cols, base_checker_cols, sizeof(checker_cols) );
    memcpy( side_cols, base_side_cols, sizeof(side_cols) );
    memcpy( solid_cols, base_solid_cols, sizeof(solid_cols) );

    // Build the shape
    m_pGeom = geom_table[config.geom];
    m_pGeom->init( m_pGeom );
    InitVlen( m_pGeom, m_pGeom->total_pts, m_pGeom->pts );

    m_fSf = 0.0f;
    m_fSfInc = m_pGeom->sf_inc;
    UpdatePts( m_pGeom, 0.0f );
}




//-----------------------------------------------------------------------------
// Name: SetupTrackbar()
// Desc: Sets the range, position, page size and line size of a trackbar.
//-----------------------------------------------------------------------------
void SetupTrackbar( HWND hDlg, int item, int lo, int hi, int lineSize, int pageSize, int pos )
{
    SendDlgItemMessage( hDlg, item, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(lo, hi) );
    SendDlgItemMessage( hDlg, item, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)pos );
    SendDlgItemMessage( hDlg, item, TBM_SETPAGESIZE, (WPARAM)0, (LPARAM)pageSize );
    SendDlgItemMessage( hDlg, item, TBM_SETLINESIZE, (WPARAM)0, (LPARAM)lineSize );
}




//-----------------------------------------------------------------------------
// Name: GetTrackbarPos()
// Desc:
//-----------------------------------------------------------------------------
int GetTrackbarPos( HWND hDlg, int item )
{
    return (int)SendDlgItemMessage( hDlg, item, TBM_GETPOS, 0, 0 );
}




//-----------------------------------------------------------------------------
// Name: ScreenSaverConfigureDialog()
// Desc: Dialog procedure of the settings dialog.
//-----------------------------------------------------------------------------
INT_PTR CALLBACK CFlowerBoxScreensaver::ScreenSaverConfigureDialog( HWND hDlg, UINT message,
                                                                    WPARAM wParam, LPARAM lParam )
{
    switch( message )
    {
    case WM_INITDIALOG:
        {
            HWND hCombo;
            TCHAR szGeom[NGEOM][20];
            int i;

            InitCommonControls();

            g_pMyFlowerBoxScreensaver->ReadSettings();

            temp_config = config;

            CheckRadioButton( hDlg, ID_COL_CHECKER, ID_COL_SINGLE, config.color_pick );
            CheckDlgButton( hDlg, ID_COL_SMOOTH, config.smooth_colors );
            CheckDlgButton( hDlg, ID_COL_TRIANGLE, config.triangle_colors );
            CheckDlgButton( hDlg, ID_COL_CYCLE, config.cycle_colors );
            CheckDlgButton( hDlg, ID_SPIN, config.spin );
            CheckDlgButton( hDlg, ID_BLOOM, config.bloom );
            CheckDlgButton( hDlg, ID_TWO_SIDED, config.two_sided == GL_FRONT_AND_BACK );

            SetupTrackbar( hDlg, DLG_SETUP_SUBDIV, MINSUBDIV, MAXSUBDIV,
                           complexity_range.step, complexity_range.page_size,
                           config.subdiv );
            SetupTrackbar( hDlg, DLG_SETUP_SIZE, MINIMAGESIZE, MAXIMAGESIZE,
                           image_size_range.step, image_size_range.page_size,
                           config.image_size );

            hCombo = GetDlgItem( hDlg, ID_GEOM );
            SendMessage( hCombo, CB_RESETCONTENT, 0, 0 );
            for( i = 0; i < NGEOM; i++ )
            {
                LoadString( NULL, IDS_GEOM_CUBE+i, szGeom[i], 20 );
                SendMessage( hCombo, CB_ADDSTRING, 0, (LPARAM)szGeom[i] );
            }
            SendMessage( hCombo, CB_SETCURSEL, config.geom, 0 );

            SetFocus( GetDlgItem( hDlg, DLG_SETUP_SUBDIV ) );
        }
        return FALSE;

    case WM_COMMAND:
        switch( LOWORD(wParam) )
        {
        case ID_COL_CHECKER:
        case ID_COL_PER_SIDE:
        case ID_COL_SINGLE:
            temp_config.color_pick = LOWORD(wParam);
            break;

        case ID_COL_SMOOTH:
            temp_config.smooth_colors = !temp_config.smooth_colors;
            break;

        case ID_COL_TRIANGLE:
            temp_config.triangle_colors = !temp_config.triangle_colors;
            break;

        case ID_COL_CYCLE:
            temp_config.cycle_colors = !temp_config.cycle_colors;
            break;

        case ID_SPIN:
            temp_config.spin = !temp_config.spin;
            break;

        case ID_BLOOM:
            temp_config.bloom = !temp_config.bloom;
            break;

        case ID_TWO_SIDED:
            temp_config.two_sided = temp_config.two_sided == GL_FRONT_AND_BACK ?
                GL_FRONT : GL_FRONT_AND_BACK;
            break;

        case IDC_SCREENSETTINGS:
            g_pMyFlowerBoxScreensaver->DoScreenSettingsDialog( hDlg );
            break;

        case IDOK:
            temp_config.subdiv = GetTrackbarPos( hDlg, DLG_SETUP_SUBDIV );
            temp_config.image_size = GetTrackbarPos( hDlg, DLG_SETUP_SIZE );
            temp_config.geom = (int)SendMessage( GetDlgItem( hDlg, ID_GEOM ), CB_GETCURSEL, 0, 0 );
            // Fall through

        case IDCANCEL:
            EndDialog( hDlg, LOWORD(wParam) );
            break;
        }
        return TRUE;
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: SetMaterialColor()
// Desc: Diffuse and ambient color of the material; white specular
//       highlights.
//-----------------------------------------------------------------------------
HRESULT CFlowerBoxScreensaver::SetMaterialColor( FLOAT* pfColors )
{
    D3DMATERIAL8 mtrl;

    ZeroMemory( &mtrl, sizeof(mtrl) );
    mtrl.Diffuse.r = pfColors[0];
    mtrl.Diffuse.g = pfColors[1];
    mtrl.Diffuse.b = pfColors[2];
    mtrl.Diffuse.a = pfColors[3];
    mtrl.Ambient = mtrl.Diffuse;
    mtrl.Specular.r = 0.8f;
    mtrl.Specular.g = 0.8f;
    mtrl.Specular.b = 0.8f;
    mtrl.Specular.a = 1.0f;
    mtrl.Power = 30.0f;

    return m_pd3dDevice->SetMaterial( &mtrl );
}




//-----------------------------------------------------------------------------
// Name: ss_RegistrySetup()
// Desc: Looks up the control.ini section of the OpenGL version of the
//       screen saver.  Returns TRUE if it exists.
//-----------------------------------------------------------------------------
BOOL CFlowerBoxScreensaver::ss_RegistrySetup( int section, int file )
{
    TCHAR szSection[100];

    if( LoadString( m_hInstance, section, g_szSectName, 255 ) &&
        LoadString( m_hInstance, file, g_szFname, 255 ) )
    {
        if( GetPrivateProfileSection( g_szSectName, szSection, 100, g_szFname ) > 0 )
            return TRUE;
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: ss_GetRegistryInt()
// Desc: Reads an integer from the control.ini section (0 if the string
//       resource with its name cannot be loaded).
//-----------------------------------------------------------------------------
int CFlowerBoxScreensaver::ss_GetRegistryInt( int name, int iDefault )
{
    TCHAR szName[256];

    if( LoadString( m_hInstance, name, szName, 255 ) )
        return GetPrivateProfileInt( g_szSectName, szName, iDefault, g_szFname );

    return 0;
}




//-----------------------------------------------------------------------------
// Name: DoConfig()
// Desc:
//-----------------------------------------------------------------------------
VOID CFlowerBoxScreensaver::DoConfig()
{
    if( IDOK == DialogBox( NULL, MAKEINTRESOURCE(DLG_SCRNSAVECONFIGURE),
                           m_hWndParent, ScreenSaverConfigureDialog ) )
    {
        NewConfig( &temp_config );
    }
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CFlowerBoxScreensaver::FrameMove()
{
    FLOAT fOldX, fOldY;

    if( m_fViewportWidth == 0.0f )
    {
        // First frame: the window of the shape is a square, image_size
        // percent of the way from a quarter to three quarters of the
        // smaller side of the screen(s), centred
        RECT rc = m_rcRenderTotal;
        DWORD dwWidth = rc.right - rc.left;
        DWORD dwHeight = rc.bottom - rc.top;
        FLOAT fSize = config.image_size * (1.0f/150.0f) * 0.75f + 0.25f;
        DWORD dwSize = (DWORD)(min( dwWidth, dwHeight ) * fSize);

        if( dwSize > dwWidth )
            dwSize = dwWidth;
        if( dwSize > dwHeight )
            dwSize = dwHeight;

        m_fViewportX = rc.left + (dwWidth - dwSize) * 0.5f;
        m_fViewportY = rc.top + (dwHeight - dwSize) * 0.5f;
        m_fViewportWidth = (FLOAT)dwSize;
        m_fViewportHeight = (FLOAT)dwSize;
        m_fViewportXVelocity = dwSize * 0.01f;
        m_fViewportYVelocity = dwSize * 0.01f;
    }

    // Bounce the window around the screen
    fOldX = m_fViewportX;
    fOldY = m_fViewportY;
    m_fViewportX += m_fViewportXVelocity * m_fElapsedTime * 20.0f;
    m_fViewportY += m_fViewportYVelocity * m_fElapsedTime * 20.0f;
    if( ( m_fViewportXVelocity < 0.0f && m_fViewportX < m_rcRenderTotal.left ) ||
        ( m_fViewportXVelocity > 0.0f && m_fViewportX + m_fViewportWidth > m_rcRenderTotal.right ) )
    {
        m_fViewportX = fOldX;
        m_fViewportXVelocity = -m_fViewportXVelocity;
    }
    if( ( m_fViewportYVelocity < 0.0f && m_fViewportY < m_rcRenderTotal.top ) ||
        ( m_fViewportYVelocity > 0.0f && m_fViewportY + m_fViewportHeight > m_rcRenderTotal.bottom ) )
    {
        m_fViewportY = fOldY;
        m_fViewportYVelocity = -m_fViewportYVelocity;
    }

    if( config.spin )
    {
        m_fXRot += m_fElapsedTime * 60.0f;
        m_fYRot += m_fElapsedTime * 40.0f;
    }

    if( config.bloom )
    {
        m_fSf += m_fSfInc * m_fElapsedTime * 20.0f;
        if( ( m_fSfInc > 0.0f && m_fSf > m_pGeom->max_sf ) ||
            ( m_fSfInc < 0.0f && m_fSf < m_pGeom->min_sf ) )
        {
            m_fSfInc = -m_fSfInc;
        }
        UpdatePts( m_pGeom, m_fSf );
    }

    if( config.cycle_colors )
    {
        ComputeHsvColors();
        m_fHueAngle += m_fElapsedTime * (50.0 * PI / 180.0);   // 50 degrees per second
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CFlowerBoxScreensaver::RestoreDeviceObjects()
{
    if( m_pd3dDevice != NULL )
    {
        D3DXMATRIX matView;
        D3DXVECTOR3 vUpVec( 0.0f, 1.0f, 0.0f );
        D3DXVECTOR3 vLookatPt( 0.0f, 0.0f, 0.0f );
        D3DXVECTOR3 vEyePt( 0.0f, 0.0f, 3.5f );

        D3DXMatrixLookAtLH( &matView, &vEyePt, &vLookatPt, &vUpVec );
        m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

        D3DLIGHT8 light;
        ZeroMemory( &light, sizeof(light) );
        light.Type = D3DLIGHT_POINT;
        light.Diffuse.r = 1.0f;
        light.Diffuse.g = 1.0f;
        light.Diffuse.b = 1.0f;
        light.Diffuse.a = 1.0f;
        light.Specular.r = 1.0f;
        light.Specular.g = 1.0f;
        light.Specular.b = 1.0f;
        light.Specular.a = 1.0f;
        light.Position.x = 2.0f;
        light.Position.y = 2.0f;
        light.Position.z = 10.0f;
        light.Range = 1000.0f;
        light.Attenuation0 = 1.0f;
        m_pd3dDevice->SetLight( 0, &light );
        m_pd3dDevice->LightEnable( 0, TRUE );

        m_pd3dDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0x40404040 );
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE,
            config.two_sided == GL_FRONT_AND_BACK ? D3DCULL_NONE : D3DCULL_CW );

        NewConfig( &config );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DrawGeom()
// Desc: Draws the shape: each side as one triangle list (two for the
//       checkerboard, one per color).
//-----------------------------------------------------------------------------
VOID CFlowerBoxScreensaver::DrawGeom( GEOMETRY* geom )
{
    int side, strip, vt;
    int* sz;
    WORD* idx;
    int pt;

    m_pd3dDevice->SetRenderState( D3DRS_SHADEMODE,
        config.smooth_colors ? D3DSHADE_GOURAUD : D3DSHADE_FLAT );

    if( config.color_pick == ID_COL_SINGLE )
        SetMaterialColor( solid_cols );

    for( pt = 0; pt < geom->total_pts; pt++ )
    {
        geom->vertices[pt].p = geom->npts[pt];
        D3DXVec3Normalize( &geom->vertices[pt].n, &geom->normals[pt] );
    }

    m_pd3dDevice->SetVertexShader( D3DFVF_MYVERTEX );

    WORD* pIndices = new WORD[MAX_TRI_INDICES];   // triangles of color 0
    if( pIndices == NULL )
        return;
    WORD* pIndices2 = new WORD[MAX_TRI_INDICES];  // triangles of color 1 (checkerboard)
    if( pIndices2 == NULL )
    {
        delete [] pIndices;
        return;
    }

    for( side = 0; side < geom->nsides; side++ )
    {
        int nIndices = 0;
        int nIndices2 = 0;
        BOOL bColor = TRUE;     // FALSE: second checkerboard color

        sz = geom->sides[side].strip_size;
        idx = geom->sides[side].strip_index;
        for( strip = 0; strip < geom->sides[side].nstrips; strip++ )
        {
            int nTri = !config.triangle_colors;
            BOOL bFlip = FALSE;

            if( config.color_pick == ID_COL_CHECKER )
                bColor = strip & 1;

            // Turn the strip into a list of triangles, all wound the same way
            for( vt = 0; vt < sz[strip]-2; vt++ )
            {
                // Change color every two triangles
                if( config.color_pick == ID_COL_CHECKER )
                {
                    nTri++;
                    if( nTri == 2 )
                    {
                        bColor = !bColor;
                        nTri = 0;
                    }
                }

                if( !bColor )
                {
                    if( bFlip )
                    {
                        pIndices2[nIndices2] = idx[1];
                        pIndices2[nIndices2+1] = idx[0];
                    }
                    else
                    {
                        pIndices2[nIndices2] = idx[0];
                        pIndices2[nIndices2+1] = idx[1];
                    }
                    pIndices2[nIndices2+2] = idx[2];
                    nIndices2 += 3;
                }
                else
                {
                    if( bFlip )
                    {
                        pIndices[nIndices] = idx[1];
                        pIndices[nIndices+1] = idx[0];
                    }
                    else
                    {
                        pIndices[nIndices] = idx[0];
                        pIndices[nIndices+1] = idx[1];
                    }
                    pIndices[nIndices+2] = idx[2];
                    nIndices += 3;
                }

                idx++;
                bFlip = !bFlip;
            }
            idx += 2;
        }

        if( config.color_pick == ID_COL_PER_SIDE )
            SetMaterialColor( side_cols[side] );
        else if( config.color_pick == ID_COL_CHECKER )
            SetMaterialColor( checker_cols[side][0] );

        m_pd3dDevice->DrawIndexedPrimitiveUP( D3DPT_TRIANGLELIST, 0, geom->total_pts,
            nIndices / 3, pIndices, D3DFMT_INDEX16, geom->vertices, sizeof(MYVERTEX) );

        if( config.color_pick == ID_COL_CHECKER )
        {
            SetMaterialColor( checker_cols[side][1] );
            m_pd3dDevice->DrawIndexedPrimitiveUP( D3DPT_TRIANGLELIST, 0, geom->total_pts,
                nIndices2 / 3, pIndices2, D3DFMT_INDEX16, geom->vertices, sizeof(MYVERTEX) );
        }
    }

    delete [] pIndices;
    delete [] pIndices2;
}




//-----------------------------------------------------------------------------
// Name: ss_ReadSettings()
// Desc: Reads the settings of the OpenGL version from control.ini, so that
//       they carry over after an upgrade.
//-----------------------------------------------------------------------------
VOID CFlowerBoxScreensaver::ss_ReadSettings()
{
    if( ss_RegistrySetup( IDS_SAVERNAME, IDS_INIFILE ) )
    {
        config.smooth_colors = ss_GetRegistryInt( IDS_CONFIG_SMOOTH_COLORS, config.smooth_colors );
        config.triangle_colors = ss_GetRegistryInt( IDS_CONFIG_TRIANGLE_COLORS, config.triangle_colors );
        config.cycle_colors = ss_GetRegistryInt( IDS_CONFIG_CYCLE_COLORS, config.cycle_colors );
        config.spin = ss_GetRegistryInt( IDS_CONFIG_SPIN, config.spin );
        config.bloom = ss_GetRegistryInt( IDS_CONFIG_BLOOM, config.bloom );
        config.subdiv = ss_GetRegistryInt( IDS_CONFIG_SUBDIV, config.subdiv );
        config.color_pick = ss_GetRegistryInt( IDS_CONFIG_COLOR_PICK, config.color_pick );
        config.image_size = ss_GetRegistryInt( IDS_CONFIG_IMAGE_SIZE, config.image_size );
        config.geom = ss_GetRegistryInt( IDS_CONFIG_GEOM, config.geom );
        config.two_sided = ss_GetRegistryInt( IDS_CONFIG_TWO_SIDED, config.two_sided );
    }
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CFlowerBoxScreensaver::Render()
{
    D3DVIEWPORT8 vp;
    RECT rcView;        // window of the shape, in screen coordinates
    RECT rcVisible;     // the part of it on this device's screen
    D3DXMATRIX matProj;

    // Clear the whole render target
    vp.X = 0;
    vp.Y = 0;
    vp.Width = m_rcRenderCurDevice.right - m_rcRenderCurDevice.left;
    vp.Height = m_rcRenderCurDevice.bottom - m_rcRenderCurDevice.top;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    m_pd3dDevice->SetViewport( &vp );

    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1.0f, 0L );

    rcView.left = (INT)m_fViewportX;
    rcView.top = (INT)m_fViewportY;
    rcView.right = rcView.left + (INT)m_fViewportWidth;
    rcView.bottom = rcView.top + (INT)m_fViewportHeight;

    if( !IntersectRect( &rcVisible, &rcView, &m_rcRenderCurDevice ) )
        return S_OK;

    // Device coordinates
    OffsetRect( &rcView, -m_rcRenderCurDevice.left, -m_rcRenderCurDevice.top );
    OffsetRect( &rcVisible, -m_rcRenderCurDevice.left, -m_rcRenderCurDevice.top );

    vp.X = rcVisible.left;
    vp.Y = rcVisible.top;
    vp.Width = rcVisible.right - rcVisible.left;
    vp.Height = rcVisible.bottom - rcVisible.top;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    m_pd3dDevice->SetViewport( &vp );

    // Project onto the visible part of the window only, so that a window
    // that straddles two screens is drawn in two pieces
    FLOAT xc = (rcView.left + rcView.right) * 0.5f;
    FLOAT yc = (rcView.top + rcView.bottom) * 0.5f;
    D3DXMatrixPerspectiveOffCenterLH( &matProj,
        (rcVisible.left - xc) / (rcView.left - xc) * -0.8f,
        (rcVisible.right - xc) / (rcView.right - xc) * 0.8f,
        (rcVisible.bottom - yc) / (rcView.bottom - yc) * 0.8f,
        (rcVisible.top - yc) / (rcView.top - yc) * -0.8f,
        2.0f, 5.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Begin the scene
    if( SUCCEEDED( m_pd3dDevice->BeginScene() ) )
    {
        D3DXMATRIX matRotX, matRotY, matRotZ;

        D3DXMatrixRotationX( &matRotX, D3DXToRadian( m_fXRot ) );
        D3DXMatrixRotationY( &matRotY, D3DXToRadian( m_fYRot ) );
        D3DXMatrixRotationZ( &matRotZ, D3DXToRadian( m_fZRot ) );

        D3DXMATRIX matWorld = matRotX * matRotY * matRotZ;
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

        DrawGeom( m_pGeom );

        // End the scene.
        m_pd3dDevice->EndScene();
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ReadSettings()
// Desc:
//-----------------------------------------------------------------------------
VOID CFlowerBoxScreensaver::ReadSettings()
{
    HKEY hkey;

    // Read the OpenGL settings first, so OS upgrade cases work
    ss_ReadSettings();

    if( ERROR_SUCCESS == RegCreateKeyEx( HKEY_CURRENT_USER, m_strRegPath,
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL ) )
    {
        DXUtil_ReadBoolRegKey( hkey, TEXT("Smooth"), &config.smooth_colors, config.smooth_colors );
        DXUtil_ReadBoolRegKey( hkey, TEXT("Slanted"), &config.triangle_colors, config.triangle_colors );
        DXUtil_ReadBoolRegKey( hkey, TEXT("Cycle"), &config.cycle_colors, config.cycle_colors );
        DXUtil_ReadBoolRegKey( hkey, TEXT("Spin"), &config.spin, config.spin );
        DXUtil_ReadBoolRegKey( hkey, TEXT("Bloom"), &config.bloom, config.bloom );
        DXUtil_ReadIntRegKey( hkey, TEXT("Subdiv"), (DWORD*)&config.subdiv, config.subdiv );
        if( config.subdiv < MINSUBDIV )
            config.subdiv = MINSUBDIV;
        if( config.subdiv > MAXSUBDIV )
            config.subdiv = MAXSUBDIV;
        DXUtil_ReadIntRegKey( hkey, TEXT("ColorPick"), (DWORD*)&config.color_pick, config.color_pick );
        DXUtil_ReadIntRegKey( hkey, TEXT("ImageSize"), (DWORD*)&config.image_size, config.image_size );
        DXUtil_ReadIntRegKey( hkey, TEXT("Geom"), (DWORD*)&config.geom, config.geom );
        DXUtil_ReadIntRegKey( hkey, TEXT("TwoSided"), (DWORD*)&config.two_sided, config.two_sided );

        ReadScreenSettings( hkey );

        RegCloseKey( hkey );
    }
}
