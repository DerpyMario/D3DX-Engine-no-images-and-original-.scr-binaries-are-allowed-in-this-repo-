/*-----------------------------------------------------------------------------
 * File: bezier.h
 *
 * Desc: Resource identifiers of the Beziers screen saver (ssbezier.scr).
 *       The numbers are those of the binary; the names are reconstructions.
 *---------------------------------------------------------------------------*/
#ifndef _BEZIER_H
#define _BEZIER_H

/* controls of the configuration dialog (DLG_SCRNSAVECONFIGURE) */
#define ID_SPEED            100     /* scroll bar "Speed" */
#define ID_FAST             104
#define ID_SLOW             105
#define ID_LENGTH           2004    /* edit "Beziers in each loop" */
#define ID_LENGTHARROW      2005    /* its up-down control */
#define ID_WIDTH            2006    /* edit "Repeat each loop" */
#define ID_WIDTHARROW       2007
#define ID_LENGTH_LABEL     2008
#define ID_WIDTH_LABEL      2009    /* (only in the help table: the label has no id) */

/* topics of display.hlp */
#define IDH_DISPLAY_SCREENSAVER_BEZIER_LENGTH   4213
#define IDH_DISPLAY_SCREENSAVER_BEZIER_WIDTH    4214
#define IDH_DISPLAY_SCREENSAVER_BEZIER_SPEED    4215

#endif /* _BEZIER_H */
