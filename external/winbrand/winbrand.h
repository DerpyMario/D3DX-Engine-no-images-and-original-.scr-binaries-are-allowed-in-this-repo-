//-----------------------------------------------------------------------------
// File: winbrand.h
//
// Desc: Declarations for the two functions of the Windows branding DLL
//       (%SystemRoot%\System32\winbrand.dll, Windows Vista and later) that the
//       screen savers call.  The DLL has no public header or import library;
//       the prototypes below were derived from the call sites in logon.scr and
//       ssText3d.scr.  winbrand.def (next to this file) produces the import
//       library.
//
//       This file is not part of the original source tree.
//-----------------------------------------------------------------------------
#ifndef _WINBRAND_H
#define _WINBRAND_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// LoadImage() for a resource of one of the branding resource DLLs
// (%SystemRoot%\Branding\<pszBrandName>\<pszBrandName>.dll, e.g. "Shellbrd").
HANDLE WINAPI BrandingLoadImage(LPCWSTR pszBrandName, UINT uID, UINT uType, int cx, int cy, UINT fuLoad);

// Expands the branding placeholders (%WINDOWS_LONG%, %WINDOWS_SHORT%, ...) of
// a string.  The result is allocated with GlobalAlloc(); free it with
// GlobalFree().
LPWSTR WINAPI BrandingFormatString(LPCWSTR pszFormat);

#ifdef __cplusplus
}
#endif

#endif // _WINBRAND_H
