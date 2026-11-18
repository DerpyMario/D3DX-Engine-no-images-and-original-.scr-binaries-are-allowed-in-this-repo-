/*-----------------------------------------------------------------------------
 * File: comstr.h
 *
 * Desc: Strings shared by the classic GDI screen savers of Windows Server
 *       2003 / Windows XP x64 (Starfield, Mystify, Beziers, Marquee).
 *
 *       Every one of them has the same six strings in its string table
 *       (comstr.rc) and loads them into buffers of the same sizes when it
 *       reads its settings.  The identifiers and buffer sizes are those of
 *       the binaries; the names are chosen by this reconstruction.
 *
 *       The settings themselves live in the [Screen Saver.<Name>] section
 *       of control.ini, which Windows NT maps to the registry key
 *       HKEY_CURRENT_USER\Control Panel\Screen Saver.<Name>.
 *---------------------------------------------------------------------------*/
#ifndef _COMSTR_H
#define _COMSTR_H

/* string identifiers (IDS_DESCRIPTION, 1, is defined by <scrnsave.h>) */
#define idsName             100     /* the name of the saver, e.g. "Stars" */
#define idsIniFile          1001    /* "control.ini" */
#define idsScreenSaver      1002    /* "ScreenSaver" */
#define idsAppName          1007    /* "Screen Saver.<Name>": the control.ini section */
#define idsNoHelpMemory     1008    /* "Your computer does not have enough memory ..." */
#define idsHelpFile         1009    /* "display.hlp" */
#define idsScreenSaverTitle 1010    /* "Screen Saver" (not loaded by any of the savers) */

/* buffer sizes, in characters */
#define TITLEBARNAMELEN     40
#define APPNAMEBUFFERLEN    40
#define MAXFILELEN          13
#define SCREENSAVERLEN      22
#define BUFFLEN             255

#ifndef RC_INVOKED

extern TCHAR szName[TITLEBARNAMELEN];
extern TCHAR szAppName[APPNAMEBUFFERLEN];
extern TCHAR szIniFile[MAXFILELEN];
extern TCHAR szScreenSaver[SCREENSAVERLEN];
extern TCHAR szHelpFile[MAXFILELEN];
extern TCHAR szNoHelpMemory[BUFFLEN];

#endif /* RC_INVOKED */

#endif /* _COMSTR_H */
