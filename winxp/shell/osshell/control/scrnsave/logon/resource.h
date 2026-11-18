/*-----------------------------------------------------------------------------
 * File: resource.h
 *
 * Desc: Resource identifiers of the logo screen saver (logon.scr) of Windows
 *       Server 2003 / Windows XP x64.  The numbers are those of logon.scr
 *       5.2.3790.3959; the names are reconstructions (each bitmap is named
 *       after the IsOS() test in OnCreateSS() that selects it).
 *
 *       IDS_DESCRIPTION (1) and DLG_SCRNSAVECONFIGURE (2003) are defined by
 *       <scrnsave.h>.
 *---------------------------------------------------------------------------*/
#ifndef _LOGON_RESOURCE_H
#define _LOGON_RESOURCE_H

/* the logos, 275 x 174 pixels (8 bits per pixel, the two XP logos 24) */
#define IDB_LOGO_STANDARD           200     /* OS_SERVER: Standard Edition */
#define IDB_LOGO_PROFESSIONAL       201     /* none of the others: Windows XP Professional */
#define IDB_LOGO_ENTERPRISE         202     /* OS_ADVSERVER: Enterprise Edition */
#define IDB_LOGO_DATACENTER         203     /* OS_DATACENTER: Datacenter Edition */
#define IDB_LOGO_HOME               204     /* OS_HOME: Windows XP Home Edition */
#define IDB_LOGO_EMBEDDED           205     /* OS_EMBEDDED: Windows XP Embedded */
#define IDB_LOGO_SMALLBUSINESS      206     /* OS_SMALLBUSINESSSERVER: Small Business Server */
#define IDB_LOGO_WEB                207     /* OS_WEBSERVER: Web Edition */
#define IDB_LOGO_TABLETPC           208     /* OS_TABLETPC: Windows XP Tablet PC Edition */
#define IDB_LOGO_APPLIANCE          212     /* OS_APPLIANCE: server appliance */

/* icons, 32 x 32 (the 16 colour one comes first in the script: icon image 1) */
#define IDI_LOGON                   209     /* 32 bits per pixel */
#define IDI_LOGON_16COLOR           210     /* 16 colours */

#define IDS_NOOPTIONS               211     /* "This screen saver has no options that you can set." */

/* the side-by-side manifest of the NT build (SXS_MANIFEST_RESOURCE_ID) */
#define IDR_SXS_MANIFEST            123

#endif /* _LOGON_RESOURCE_H */
