/*-----------------------------------------------------------------------------
 * File: mystify.h
 *
 * Desc: Resource identifiers of the Mystify screen saver (ssmyst.scr).  The
 *       numbers are those of the binary; the names are reconstructions.
 *---------------------------------------------------------------------------*/
#ifndef _MYSTIFY_H
#define _MYSTIFY_H

#define idsPolygon          2       /* "Polygon %d" */

/* controls of the configuration dialog (DLG_SCRNSAVECONFIGURE) */
#define ID_SHAPE            100     /* combo box "Shape": Polygon 1 / Polygon 2 */
#define ID_2COLORS          102     /* radio "Two Colors" */
#define ID_RANDOMCOLORS     103     /* radio "Multiple Random Colors" */
#define ID_LINES            104     /* edit "Lines" */
#define ID_LINESARROW       105     /* its up-down control */
#define ID_ACTIVE           106     /* check box "Active" */
#define ID_CLEARSCREEN      121     /* check box "Clear Screen" */
#define ID_COLOR1           122     /* owner drawn colour lists */
#define ID_COLOR2           123
#define ID_COLORGROUP       124     /* group box "Colors To Use" */
#define ID_SHAPE_LABEL      128
#define ID_LINES_LABEL      129

/* topics of display.hlp */
#define IDH_DISPLAY_SCREENSAVER_MYSTIFY_SHAPE           4207
#define IDH_DISPLAY_SCREENSAVER_MYSTIFY_ACTIVE          4208
#define IDH_DISPLAY_SCREENSAVER_MYSTIFY_LINES           4209
#define IDH_DISPLAY_SCREENSAVER_MYSTIFY_2COLORS         4210
#define IDH_DISPLAY_SCREENSAVER_MYSTIFY_RANDOMCOLORS    4211
#define IDH_DISPLAY_SCREENSAVER_MYSTIFY_CLEARSCREEN     4212

#endif /* _MYSTIFY_H */
