/* screen.h — direct screen-memory scroll for the body editor.
 *
 * scr_scroll() shifts the editor's edit area up or down by exactly one text
 * row by moving character cells in screen memory, instead of repainting the
 * whole screen from the text buffer.  The cells are already rendered, so the
 * caller only has to draw the one (or two, after a word-wrap reflow) rows that
 * change — turning an O(document) scroll into one memory move plus a row.
 *
 * Implemented in screen.s, which lives in the language card (like qrcode.c)
 * because main RAM is full.  See main.c: body_editor().
 */
#ifndef SCREEN_H
#define SCREEN_H

/* Flags OR'd into the scr_scroll() argument (low 5 bits carry the row count). */
#define SCR_W80   0x20   /* 80-column: also move the odd columns in aux memory */
#define SCR_DOWN  0x40   /* scroll DOWN (new row at top); clear = UP            */

#ifdef __CC65__
/* Scroll the edit area (screen rows 2 .. 2+rows-1, i.e. starting at WP_HDR) by
   one text row.  `arg` = rows | SCR_W80? | SCR_DOWN?.  rows must be >= 2 and
   fit in 5 bits.  Handles the 80-column main/aux split internally. */
void __fastcall__ scr_scroll(unsigned char arg);
#endif

#endif /* SCREEN_H */
