//-----------------------------------------------------------------------------
// File: resource.h
//
// Desc: Resource identifiers of the Windows Logo screen saver (logon.scr).
//       The numeric values come from logon.scr / logon.scr.mui; the symbolic
//       names of the strings are reconstructions.
//
//       The 90 flare bitmaps are not identified by number but by name:
//       "IDB_FLARE01" ... "IDB_FLARE45" and "IDB_SMALLFLARE01" ...
//       "IDB_SMALLFLARE45" (the original resource script used the names
//       without defining them, which makes them string names).
//
//       IDS_DESCRIPTION (1) and DLG_SCRNSAVECONFIGURE (2003) are defined by
//       <scrnsave.h>.
//-----------------------------------------------------------------------------
#ifndef _LOGON_RESOURCE_H
#define _LOGON_RESOURCE_H

// Strings of the "this screen saver has no options" task dialog
#define IDS_CONFIG_NOOPTIONS            213
#define IDS_CONFIG_NOOPTIONS_TEXT       214
#define IDS_CONFIG_TITLE                215

#endif // _LOGON_RESOURCE_H
