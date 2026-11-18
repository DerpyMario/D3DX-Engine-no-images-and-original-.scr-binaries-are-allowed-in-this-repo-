/*-----------------------------------------------------------------------------
 * File: comstr.c
 *
 * Desc: The buffers of the shared strings of the classic GDI screen savers
 *       (see comstr.h).  Each saver loads them itself, in its
 *       GetIniEntries() function.
 *---------------------------------------------------------------------------*/
#include <windows.h>
#include "comstr.h"

TCHAR szName[TITLEBARNAMELEN];          /* idsName */
TCHAR szAppName[APPNAMEBUFFERLEN];      /* idsAppName: the control.ini section */
TCHAR szIniFile[MAXFILELEN];            /* idsIniFile */
TCHAR szScreenSaver[SCREENSAVERLEN];    /* idsScreenSaver */
TCHAR szHelpFile[MAXFILELEN];           /* idsHelpFile */
TCHAR szNoHelpMemory[BUFFLEN];          /* idsNoHelpMemory */
