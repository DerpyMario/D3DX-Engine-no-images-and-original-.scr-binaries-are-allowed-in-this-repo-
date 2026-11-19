//-----------------------------------------------------------------------------
// File: stdafx.h
//
// Desc: Precompiled header of the 3D Pipes screen saver (stdafx.obj).
//-----------------------------------------------------------------------------
#ifndef _STDAFX_H
#define _STDAFX_H

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timeb.h>
#include <d3d8.h>
#include <d3dx8.h>
#include "../d3dsaver/d3dsaver.h"
#include "../d3dsaver/dxutil.h"
#include "sspipes.h"
#include "pipes.h"
#include "resource.h"

#ifndef min     // (windef.h of the Windows SDK)
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#endif // _STDAFX_H
