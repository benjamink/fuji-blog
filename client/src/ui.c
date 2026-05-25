/* ──────────────────────────────────────────────────────────────
   FujiBlogger — screen-layout primitives  (ui.c)
   ────────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <string.h>
#include "ui.h"

#ifdef __CC65__
#include <conio.h>
#endif

/* Defined in main.c */
extern int screen_width;

/* ── ui_header ──────────────────────────────────────────────── */
/*
 * Draws one full-width row in inverse video:
 *   "FujiBlogger"    <spaces>   TITLE (centered)   <spaces>   hint
 *
 * Columns are filled with spaces so the entire row is highlighted.
 */
void ui_header(const char *title, const char *hint)
{
    int tlen, hlen, center_start, right_start, col, i;

    tlen         = (int)strlen(title);
    hlen         = (int)strlen(hint);
    center_start = (screen_width - tlen) / 2;
    right_start  = screen_width - hlen;
    col          = 0;

#ifdef __CC65__
    revers(1);
#else
    printf("\033[7m");
#endif

    /* Left: app name */
    printf("FujiBlogger");
    col = 11;

    /* Pad to centre of title */
    for (i = col; i < center_start; i++) { putchar(' '); col++; }

    /* Centre: title */
    printf("%s", title);
    col += tlen;

    /* Pad to right-edge of hint */
    for (i = col; i < right_start; i++) { putchar(' '); col++; }

    /* Right: hint (only if it fits) */
    if (col + hlen <= screen_width) {
        printf("%s", hint);
        col += hlen;
    }

    /* Fill remaining columns */
    for (i = col; i < screen_width; i++) putchar(' ');
    putchar('\n');

#ifdef __CC65__
    revers(0);
#else
    printf("\033[0m");
#endif
}

/* ── ui_hline ───────────────────────────────────────────────── */
void ui_hline(void)
{
    int i;
    for (i = 0; i < screen_width; i++) putchar('=');
    putchar('\n');
}

/* ── ui_indent ──────────────────────────────────────────────── */
void ui_indent(void)
{
    int i, n;
    n = (screen_width >= 80) ? 20 : 4;
    for (i = 0; i < n; i++) putchar(' ');
}

/* ── ui_post_row ────────────────────────────────────────────── */
void ui_post_row(int n, int pub, const char *title)
{
    int ind, maxw, tlen, i;

    ind  = (screen_width >= 80) ? 20 : 4;
    /* prefix "NN. [X] " = 8 chars */
    maxw = screen_width - ind - 8;
    if (maxw < 4) maxw = 4;
    tlen = (int)strlen(title);

    for (i = 0; i < ind; i++) putchar(' ');
    printf("%d. [%s] ", n, pub ? "P" : "D");
    if (tlen > maxw) {
        for (i = 0; i < maxw; i++) putchar(title[i]);
    } else {
        printf("%s", title);
    }
    putchar('\n');
}

/* ── ui_center ──────────────────────────────────────────────── */
void ui_center(const char *text, int width)
{
    int tlen, pad, i;
    tlen = (int)strlen(text);
    pad  = (width - tlen) / 2;
    if (pad < 0) pad = 0;
    for (i = 0; i < pad; i++) putchar(' ');
    printf("%s\n", text);
}

/* ── ui_fill ────────────────────────────────────────────────── */
void ui_fill(char ch, int width)
{
    int i;
    for (i = 0; i < width; i++) putchar((int)(unsigned char)ch);
    putchar('\n');
}
