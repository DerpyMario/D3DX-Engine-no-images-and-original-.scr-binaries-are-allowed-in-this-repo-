/*-----------------------------------------------------------------------------
 * File: marquee.h
 *
 * Desc: Resource identifiers of the Marquee screen saver (ssmarque.scr).
 *       The numbers are those of the binary; the names are reconstructions.
 *---------------------------------------------------------------------------*/
#ifndef _MARQUEE_H
#define _MARQUEE_H

/* strings */
#define idsDefaultText      101     /* "Windows NT %d.%d" */
#define idsFormatText       102     /* "Format Text": title of the font dialog */
#define idsDefaultFont      103     /* "Tahoma" */

/* controls of the configuration dialog (DLG_SCRNSAVECONFIGURE) */
#define ID_TEXT             102     /* edit "Text" */
#define ID_BGCOLOR          104     /* owner drawn colour list */
#define ID_CENTERED         105     /* radio buttons "Position" */
#define ID_RANDOM           106
#define ID_SPEED            107     /* scroll bar "Speed" */
#define ID_SHOWTEXT         108     /* "Text Example" window */
#define ID_FORMATTEXT       109     /* button "Format Text..." */
#define ID_BGCOLOR_LABEL    113
#define ID_SLOW             114
#define ID_FAST             115
#define ID_TEXT_LABEL       116

/* topics of display.hlp */
#define IDH_DISPLAY_SCREENSAVER_MARQUEE_CENTERED        4200
#define IDH_DISPLAY_SCREENSAVER_MARQUEE_RANDOM          4201
#define IDH_DISPLAY_SCREENSAVER_MARQUEE_SPEED           4202
#define IDH_DISPLAY_SCREENSAVER_MARQUEE_BGCOLOR         4203
#define IDH_DISPLAY_SCREENSAVER_MARQUEE_TEXT            4204
#define IDH_DISPLAY_SCREENSAVER_MARQUEE_EXAMPLE         4205
#define IDH_DISPLAY_SCREENSAVER_MARQUEE_FORMATTEXT      4206

#endif /* _MARQUEE_H */
