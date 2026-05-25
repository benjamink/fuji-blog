#ifndef UI_H
#define UI_H

/* ──────────────────────────────────────────────────────────────
   FujiBlogger — screen-layout primitives
   All functions respect the global `screen_width` variable.
   ────────────────────────────────────────────────────────────── */

/* Full-width inverse-video header bar.
   Layout:  "FujiBlogger"  |  centered title  |  right-justified hint
   On cc65 uses revers(1/0); on other platforms uses ANSI reverse video. */
void ui_header(const char *title, const char *hint);

/* Full-width '=' separator line. */
void ui_hline(void);

/* Indentation for menu / list items:  4 spaces (40-col), 20 spaces (80-col). */
void ui_indent(void);

/* Print a formatted post-list row:
      <indent>N. [P|D] title (truncated to fit)
   'pub' is 1 for published, 0 for draft. */
void ui_post_row(int n, int pub, const char *title);

/* Print text centred within width columns (no trailing newline). */
void ui_center(const char *text, int width);

/* Print a full-width line of character ch (no newline added if ch == '\n'). */
void ui_fill(char ch, int width);

#endif /* UI_H */
