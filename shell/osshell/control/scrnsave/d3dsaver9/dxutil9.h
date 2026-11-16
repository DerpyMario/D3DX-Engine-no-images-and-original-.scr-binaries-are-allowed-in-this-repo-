//-----------------------------------------------------------------------------
// File: dxutil9.h
//
// Desc: Timer helper used by the D3DSaver9 framework.
//
//       Original object:
//           shell\osshell\control\scrnsave\d3dsaver9\objfre\amd64\dxutil9.obj
//-----------------------------------------------------------------------------
#ifndef _DXUTIL9_H
#define _DXUTIL9_H

#include <windows.h>

//-----------------------------------------------------------------------------
// Miscellaneous helper macros
//-----------------------------------------------------------------------------
#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=NULL; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif

//-----------------------------------------------------------------------------
// Name: DXUtil_Timer()
// Desc: Performs timer operations. Use the following commands:
//          TIMER_START           - to start the timer
//          TIMER_GETABSOLUTETIME - to get the absolute system time
//          TIMER_GETAPPTIME      - to get the current time
//          TIMER_GETELAPSEDTIME  - to get the time that elapsed between
//                                  TIMER_GETELAPSEDTIME calls
//
//       The enumeration keeps the numbering of the DirectX SDK helper, but the
//       shipped code only implements the four commands listed above; any other
//       command returns -1.
//-----------------------------------------------------------------------------
enum TIMER_COMMAND
{
    TIMER_RESET,
    TIMER_START,
    TIMER_STOP,
    TIMER_ADVANCE,
    TIMER_GETABSOLUTETIME,
    TIMER_GETAPPTIME,
    TIMER_GETELAPSEDTIME
};

FLOAT __cdecl DXUtil_Timer( TIMER_COMMAND command );

#endif // _DXUTIL9_H
