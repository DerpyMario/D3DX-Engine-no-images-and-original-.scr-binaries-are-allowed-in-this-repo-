//-----------------------------------------------------------------------------
// File: sspipes.h
//
// Desc: The classes of the 3D Pipes engine: the drawing state, the view, the
//       grid of nodes the pipes run through, the pipes and the objects they
//       are drawn with.
//
//       3D Pipes (sspipes.scr) of Windows XP / Windows Server 2003 (x86) is
//       the Direct3D 8 port of the OpenGL screen saver of Windows NT.  The
//       class and function names are those of the symbol file; the names of
//       data members, enumerators and constants are reconstructions.
//
//       Objects (d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\):
//           eval, fpipe, fstate, material, node, npipe, nstate, objects,
//           pipe, pipes, state, stdafx, view, xc
//-----------------------------------------------------------------------------
#ifndef _SSPIPES_H
#define _SSPIPES_H

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Directions (also the indices of a node's neighbours)
enum
{
    PLUS_X = 0,
    MINUS_X,
    PLUS_Y,
    MINUS_Y,
    PLUS_Z,
    MINUS_Z,
    NUM_DIRS,
    DIR_NONE = 7,       // no direction possible: the pipe is stuck
    DIR_STRAIGHT = 8    // (TURNING_FLEX_PIPE) keep going straight
};

// Pipe status
enum { PIPE_ACTIVE = 0, PIPE_STUCK, PIPE_OUT_OF_NODES };

// Pipe types
enum { TYPE_NORMAL = 0, TYPE_FLEX_REGULAR, TYPE_FLEX_TURNING };

// How a pipe chooses its directions and its start position
enum { CHOOSE_DIR_RANDOM_WEIGHTED = 0, CHOOSE_DIR_CHASE };
enum { CHOOSE_STARTPOS_RANDOM = 0, CHOOSE_STARTPOS_FURTHEST };

// Joint types of the settings
enum { JOINT_ELBOW = 0, JOINT_BALL, JOINT_MIXED, JOINT_CYCLE };

// Joints of NORMAL_STATE
enum { ELBOWS = 0, BALLS, EITHER };
enum { ELBOW_JOINT = 0, BALL_JOINT };

// Schemes of FLEX_STATE
enum { SC_EXTRUDED_XC = 0, SC_TURNOMANIA };

// STATE::m_resetStatus
#define RESET_STARTUP_BIT   (1 << 0)
#define RESET_NORMAL_BIT    (1 << 1)
#define RESET_RESIZE_BIT    (1 << 2)
#define RESET_REPAINT_BIT   (1 << 3)

#define MAX_TEXTURES        8
#define MAX_DRAW_THREADS    4

// Evaluators of the flexible pipes
#define MAX_UORDER          5       // per section
#define MAX_VORDER          5
#define MAX_USECTIONS       4
#define MAX_XC_PTS          (MAX_UORDER * MAX_USECTIONS)
#define TEX_ORDER           2

#define ROOT_TWO            1.414213562373f
#define PI                  3.14159265358979323846f
#define PI_OVER_2           (PI / 2.0f)

// Arc control points: the distance of the control points of a Bezier
// quarter circle from its ends, as a fraction of the radius
#define EVAL_ARC_CTRL       0.56f

// Orders of the evaluators
#define EVAL_ARC_ORDER      4
#define EVAL_CYLINDER_ORDER 2


//-----------------------------------------------------------------------------
// Basic types
//-----------------------------------------------------------------------------
typedef struct _ipoint2d { int x, y; } IPOINT2D;
typedef struct _ipoint3d { int x, y, z; } IPOINT3D;

struct TEX_POINT2D { FLOAT s, t; };

// A texture and its size
struct TEXTUREINFO
{
    LPDIRECT3DTEXTURE8 pTexture;
    int                width;
    int                height;
    int                reserved;
};

// Vertex of the pipe objects
#define D3DFVF_MYVERTEX (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)

struct MYVERTEX
{
    D3DXVECTOR3 p;      // position
    D3DXVECTOR3 n;      // normal
    FLOAT       tu, tv; // texture coordinates
};

// Vertex of the quad that fades the screen out
#define D3DFVF_FADEVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)

struct FADEVERTEX
{
    FLOAT    x, y, z, rhw;
    D3DCOLOR color;
};

class STATE;
class PIPE;
class XC;


//-----------------------------------------------------------------------------
// Name: class VIEW
// Desc: The view of the grid of nodes
//-----------------------------------------------------------------------------
class VIEW
{
public:
    FLOAT       m_zTrans;       // z translation of the scene
    FLOAT       m_yRot;         // rotation of the scene about y, in degrees
    FLOAT       m_viewDist;     // viewing distance, usually -m_zTrans
    int         m_numDiv;       // number of grid divisions in x, y and z
    FLOAT       m_divSize;      // distance between the divisions
    SIZE        m_winSize;      // size of the window, in pixels
    BOOL        m_bProjMode;
    FLOAT       m_fov;          // projection
    FLOAT       m_zNear;
    FLOAT       m_zFar;
    FLOAT       m_aspectRatio;
    D3DXVECTOR3 m_world;        // size of the view area in world space

    VIEW();
    void        SetProjMatrix( IDirect3DDevice8* pd3dDevice );
    void        CalcNodeArraySize( IPOINT3D* pNumNodes );
    BOOL        SetWinSize( int width, int height );
    void        IncrementSceneRotation();
};


//-----------------------------------------------------------------------------
// Name: class NODE_ARRAY
// Desc: The nodes of the grid: a node is empty until a pipe goes through it
//-----------------------------------------------------------------------------
class Node
{
public:
    BOOL empty;
};

class NODE_ARRAY
{
private:
    Node*       m_nodes;
    int         m_lock;
    IPOINT3D    m_numNodes;
    int         m_nodeDirInc[NUM_DIRS];   // offset of the neighbour in each direction

    Node*       GetNode( IPOINT3D* pos );
    Node*       GetNextNode( IPOINT3D* pos, int dir );
    int         GetEmptyNeighbours( Node** nNode, int* nEmpty );
    int         GetEmptyTurnNeighbours( Node** nNode, int* nEmpty, int lastDir );
    int         GetEmptyNeighboursAlongDir( IPOINT3D* pos, int dir, int searchRadius );
    void        GetNeighbours( IPOINT3D* pos, Node** nNode );
    BOOL        GetNextNodePos( IPOINT3D* curPos, IPOINT3D* nextPos, int dir );

public:
    NODE_ARRAY();
    ~NODE_ARRAY();
    void        Resize( IPOINT3D* pNewSize );
    void        Reset();
    void        GetNodeCount( IPOINT3D* pos );
    void        NodeVisited( IPOINT3D* pos );
    int         FindRandomEmptyNode( IPOINT3D* pos );
    int         FindRandomEmptyNode2D( IPOINT3D* pos, int plane, int* box );
    BOOL        TakeClosestEmptyNode( IPOINT3D* newPos, IPOINT3D* pos );
    int         ChooseRandomDirection( IPOINT3D* pos, int dir, int weight );
    int         ChoosePreferredDirection( IPOINT3D* pos, int dir, int* prefDirs, int nPrefDirs );
    int         FindClearestDirection( IPOINT3D* pos );
    int         GetBestPossibleTurns( IPOINT3D* pos, int dir, int* turnDirs );
    int         ChooseNewTurnDirection( IPOINT3D* pos, int dir );
};


//-----------------------------------------------------------------------------
// Objects: vertex buffers of the parts of the pipes
//-----------------------------------------------------------------------------
class OBJECT_BUILD_INFO
{
public:
    FLOAT       radius;
    FLOAT       divSize;
    int         nSlices;
    BOOL        bTexture;
    IPOINT2D*   texRep;
};

class OBJECT
{
protected:
    int                     m_reserved[2];
    LPDIRECT3DDEVICE8       m_pd3dDevice;
    LPDIRECT3DVERTEXBUFFER8 m_pVB;
    DWORD                   m_dwNumTriangles;

public:
    OBJECT( IDirect3DDevice8* pd3dDevice );
    ~OBJECT();
    void        Draw( D3DXMATRIX* pWorldMat );
};

class PIPE_OBJECT : public OBJECT
{
private:
    void        Build( OBJECT_BUILD_INFO* state, FLOAT length, FLOAT s_start, FLOAT s_end );
public:
    PIPE_OBJECT( IDirect3DDevice8* pd3dDevice, OBJECT_BUILD_INFO* state, FLOAT length );
    PIPE_OBJECT( IDirect3DDevice8* pd3dDevice, OBJECT_BUILD_INFO* state, FLOAT length, FLOAT s_start, FLOAT s_end );
};

class ELBOW_OBJECT : public OBJECT
{
private:
    void        Build( OBJECT_BUILD_INFO* state, int notch, FLOAT s_start, FLOAT s_end );
public:
    ELBOW_OBJECT( IDirect3DDevice8* pd3dDevice, OBJECT_BUILD_INFO* state, int notch );
    ELBOW_OBJECT( IDirect3DDevice8* pd3dDevice, OBJECT_BUILD_INFO* state, int notch, FLOAT s_start, FLOAT s_end );
};

class BALLJOINT_OBJECT : public OBJECT
{
private:
    void        Build( OBJECT_BUILD_INFO* state, int notch, FLOAT s_start, FLOAT s_end );
public:
    BALLJOINT_OBJECT( IDirect3DDevice8* pd3dDevice, OBJECT_BUILD_INFO* state, int notch, FLOAT s_start, FLOAT s_end );
};

class SPHERE_OBJECT : public OBJECT
{
private:
    void        Build( OBJECT_BUILD_INFO* state, FLOAT radius, FLOAT s_start, FLOAT s_end );
public:
    SPHERE_OBJECT( IDirect3DDevice8* pd3dDevice, OBJECT_BUILD_INFO* state, FLOAT radius );
    SPHERE_OBJECT( IDirect3DDevice8* pd3dDevice, OBJECT_BUILD_INFO* state, FLOAT radius, FLOAT s_start, FLOAT s_end );
};


//-----------------------------------------------------------------------------
// Cross sections and evaluators of the flexible pipes
//-----------------------------------------------------------------------------
class XC
{
public:
    FLOAT       m_xLeft;            // bounding box
    FLOAT       m_xRight;
    FLOAT       m_yTop;
    FLOAT       m_yBottom;
    int         m_numPts;
    D3DXVECTOR2 m_pts[12];          // control points

    XC( int nPts );
    XC( XC* xc );
    ~XC() {}

    void        Scale( FLOAT scale );
    FLOAT       MaxExtent();
    FLOAT       MinTurnRadius( int relDir );
    void        CalcArcACValues90( int dir, FLOAT radius, FLOAT* acPts );
    void        CalcArcACValuesByDistance( FLOAT* acPts );
    void        ConvertPtsZ( D3DXVECTOR3* pts, FLOAT z );

protected:
    void        CalcBoundingBox();
};

class ELLIPTICAL_XC : public XC
{
private:
    void        SetControlPoints( FLOAT r1, FLOAT r2 );
public:
    ELLIPTICAL_XC( FLOAT r1, FLOAT r2 );
};

class RANDOM4ARC_XC : public XC
{
private:
    void        SetControlPoints( FLOAT radius );
public:
    RANDOM4ARC_XC( FLOAT radius );
};

class EVAL
{
public:
    int             m_numSections;  // number of cross-section sections
    int             m_uOrder;       // orders of the Bezier patches
    int             m_vOrder;
    int             m_uDiv;         // tessellation of the patches
    int             m_vDiv;
    D3DXVECTOR3*    m_pts;          // vertex control points
    BOOL            m_bTexture;
    TEX_POINT2D*    m_texPts;       // texture control points

    EVAL( BOOL bTexture );
    ~EVAL();
    void        SetTextureControlPoints( FLOAT s_start, FLOAT s_end, FLOAT t_start, FLOAT t_end );
    void        SetVertexCtrlPtsXCTranslate( D3DXVECTOR3* pts, FLOAT length, XC* xcStart, XC* xcEnd );
    void        ProcessXCPrimLinear( XC* xcStart, XC* xcEnd, FLOAT length );
    void        ProcessXCPrimBendSimple( XC* xc, int dir, FLOAT radius );
    void        ProcessXCPrimSingularity( XC* xc, FLOAT length, BOOL bOpening );
};


//-----------------------------------------------------------------------------
// Pipes
//-----------------------------------------------------------------------------
class PIPE
{
public:
    int             m_type;
    IPOINT3D        m_curPos;           // current node position
    D3DMATERIAL8*   m_pMat;
    STATE*          m_pState;
    FLOAT           m_radius;
    int             m_status;
    int             m_lastDir;          // direction the pipe went last
    int             m_notchVec;         // orientation of the pipe
    int             m_weightStraight;   // weight of going straight on
    ID3DXMatrixStack* m_pWorldMatrixStack;

    PIPE( STATE* pState );
    virtual ~PIPE();
    virtual void    Start() = 0;
    virtual void    Draw() = 0;

    void        SetChooseDirectionMethod( int method );
    void        SetChooseStartPosMethod( int method );
    BOOL        IsStuck();
    int         ChooseNewDirection();

protected:
    int         m_chooseDirMethod;
    int         m_chooseStartPosMethod;

    void        ChooseMaterial();
    BOOL        SetStartPos();
    void        TranslateToCurrentPosition();
    void        UpdateCurrentPosition( int newDir );
    void        align_plusz( int newDir );

private:
    int         GetBestDirsForChase( int* bestDirs );
};

class NORMAL_STATE;

class NORMAL_PIPE : public PIPE
{
private:
    NORMAL_STATE* m_pNState;

public:
    NORMAL_PIPE( STATE* pState );
    virtual void    Start();
    virtual void    Draw();

    void        DrawStartCap( int newDir );
    void        DrawEndCap();
    void        DrawJoint( int newDir );
    int         ChooseElbow( int oldDir, int newDir );
    void        align_plusy( int oldDir, int newDir );
    void        align_notch( int newDir, int notch );
};

class FLEX_PIPE : public PIPE
{
protected:
    XC*         m_xcCur;            // current cross section
    XC*         m_xcEnd;            // (unused)
    EVAL*       m_pEval;
    FLOAT       m_zTrans;           // distance travelled into the current node
    int         m_nSlices;          // around the circumference

    // texture coordinates (see SetTexParams)
    int         m_tangent;          // (unused)
    FLOAT       m_s_start;
    FLOAT       m_s_end;
    FLOAT       m_t_start;
    FLOAT       m_t_end;
    FLOAT       m_s_length;
    FLOAT       m_evalDivSize;

    FLEX_PIPE( STATE* pState );
    virtual ~FLEX_PIPE();

    void        ChooseXCProfile();
    void        DrawExtrudedXCObject( FLOAT length );
    void        DrawXCElbow( int newDir, FLOAT radius );
    void        DrawCap( int type );

private:
    void        CalcEvalLengthParams( FLOAT length );

public:
    void        SetTexParams( TEXTUREINFO* pTex, IPOINT2D* pTexRep );
};

class REGULAR_FLEX_PIPE : public FLEX_PIPE
{
private:
    FLOAT       m_turnFactorMin;
    FLOAT       m_turnFactorMax;

public:
    REGULAR_FLEX_PIPE( STATE* pState );
    virtual void    Start();
    virtual void    Draw();
};

class TURNING_FLEX_PIPE : public FLEX_PIPE
{
public:
    TURNING_FLEX_PIPE( STATE* pState );
    virtual void    Start();
    virtual void    Draw();
};


//-----------------------------------------------------------------------------
// The two kinds of pipe states
//-----------------------------------------------------------------------------
class NORMAL_STATE
{
public:
    int             m_jointStyle;
    BOOL            m_bCycleJointStyles;
    IDirect3DDevice8* m_pd3dDevice;
    PIPE_OBJECT*    m_shortPipe;        // between the end of a joint and the next node
    PIPE_OBJECT*    m_longPipe;         // from node to node
    ELBOW_OBJECT*   m_elbows[4];        // one per notch orientation
    SPHERE_OBJECT*  m_ballCap;          // start and end cap
    SPHERE_OBJECT*  m_bigBall;          // ball joint without texture
    BALLJOINT_OBJECT* m_ballJoints[4];  // ball joints with texture

    NORMAL_STATE( STATE* pState );
    ~NORMAL_STATE();
    void        Reset();
    int         ChooseJointType();
    void        BuildObjects( FLOAT radius, FLOAT divSize, int nSlices, BOOL bTexture, IPOINT2D* pTexRep );
};

class FLEX_STATE
{
public:
    int         m_scheme;
    STATE*      m_pMainState;

    FLEX_STATE( STATE* pState );
    void        Reset();
    BOOL        OKToUseChase();
    PIPE*       NewPipe( STATE* pState );
    int         GetMaxPipesPerFrame();
};


//-----------------------------------------------------------------------------
// Name: class STATE
// Desc: Everything about the pipes drawn on one device
//-----------------------------------------------------------------------------
struct CONFIG;

class DRAW_THREAD
{
public:
    TEXTUREINFO*        m_pTextureInfo;
    IDirect3DDevice8*   m_pd3dDevice;
    PIPE*               m_pPipe;
    int                 m_priority;

    DRAW_THREAD();
    ~DRAW_THREAD();
    BOOL        StartPipe();
    void        KillPipe();
};

class STATE
{
public:
    CONFIG*             m_pConfig;
    BOOL                m_bUseTexture;
    TEXTUREINFO         m_texInfo[MAX_TEXTURES];
    int                 m_nTextures;
    IDirect3DDevice8*   m_pd3dDevice;
    ID3DXMatrixStack*   m_pWorldMatrixStack;
    D3DLIGHT8           m_light;
    FLOAT               m_fLastTime;        // (CPipesScreensaver::Render: speed)
    PIPE*               m_pLeadPipe;        // the pipe the others chase
    int                 m_nSlices;
    IPOINT2D            m_texRep[MAX_TEXTURES];
    VIEW                m_view;
    FLOAT               m_radius;
    NODE_ARRAY*         m_nodes;
    NORMAL_STATE*       m_pNState;
    FLEX_STATE*         m_pFState;
    LPDIRECT3DVERTEXBUFFER8 m_pClearVB;     // quad that fades the screen out
    int                 m_drawScheme;       // TYPE_NORMAL or flexible pipes
    int                 m_drawMode;         // 1: chase
    int                 m_maxPipesPerFrame;
    int                 m_nPipesDrawn;
    int                 m_maxDrawThreads;
    int                 m_nDrawThreads;
    DRAW_THREAD         m_drawThreads[MAX_DRAW_THREADS];
    int                 m_resetStatus;

    STATE( CONFIG* pConfig );
    ~STATE();
    HRESULT     InitDeviceObjects( IDirect3DDevice8* pd3dDevice );
    HRESULT     RestoreDeviceObjects();
    HRESULT     InvalidateDeviceObjects();
    HRESULT     DeleteDeviceObjects();
    HRESULT     FrameMove( FLOAT fElapsedTime );
    HRESULT     Render();
    void        Repaint();

private:
    HRESULT     LoadTextureFiles( int nTextures, TCHAR strTextureFiles[][MAX_TEXTURES], int* anDefaultTextureResource );
    void        CalcTexRepFactors();
    int         PickRandomTexture( int iThread, int nTextures );
    void        ResetView();
    BOOL        FrameReset();
    BOOL        Clear();
    void        ChooseNewLeadPipe();
    void        CompactThreadList();
    int         CalcMaxPipesPerFrame();
};


//-----------------------------------------------------------------------------
// Materials
//-----------------------------------------------------------------------------
#define NUM_TEA_MATERIALS   24
#define NUM_TEX_MATERIALS   4
#define NUM_GOOD_MATERIALS  16

extern D3DMATERIAL8 g_Materials[NUM_TEA_MATERIALS + NUM_TEX_MATERIALS];
void InitMaterials();
D3DMATERIAL8* RandomTeaMaterial();
D3DMATERIAL8* RandomTexMaterial();

extern int notchTurn[NUM_DIRS][NUM_DIRS][NUM_DIRS];


#if defined(_M_IX86) || defined(__i386__)
static_assert( sizeof(VIEW) == 0x3c, "VIEW" );
static_assert( sizeof(NODE_ARRAY) == 0x2c, "NODE_ARRAY" );
static_assert( sizeof(OBJECT) == 0x14, "OBJECT" );
static_assert( sizeof(XC) == 0x74, "XC" );
static_assert( sizeof(EVAL) == 0x20, "EVAL" );
static_assert( sizeof(NORMAL_PIPE) == 0x40, "NORMAL_PIPE" );
static_assert( sizeof(TURNING_FLEX_PIPE) == 0x6c, "TURNING_FLEX_PIPE" );
static_assert( sizeof(REGULAR_FLEX_PIPE) == 0x74, "REGULAR_FLEX_PIPE" );
static_assert( sizeof(NORMAL_STATE) == 0x3c, "NORMAL_STATE" );
static_assert( sizeof(FLEX_STATE) == 8, "FLEX_STATE" );
static_assert( sizeof(STATE) == 500, "STATE" );
#endif

#endif // _SSPIPES_H
