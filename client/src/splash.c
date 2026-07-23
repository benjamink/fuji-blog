/* ──────────────────────────────────────────────────────────────────────────
   splash.c  —  Text-mode block-letter splash screen for FujiBlogger

   Builds large bitmap letters from inverse-video space characters drawn
   with gotoxy()/revers() in the existing 80-column text mode.
   Inverse ' ' = solid green block on an Apple II phosphor display.
   No mode switching, no soft switches, no graphics pages needed.

   Layout (80 × 24 text rows):
     Rows  0– 3  signal arcs ASCII art (top-left)
     Rows  4–10  "FUJI"    — inverse block letters, centered (7 rows)
     Rows 11     blank
     Rows 12–18  "BLOGGER" — inverse block letters, centered (7 rows)
     Row  19     ==== divider ====
     Row  20     blank
     Row  21     "WRITE ON THE WIRE" centered
     Row  22     blank
     Row  23     "Press any key..." right-aligned
   ────────────────────────────────────────────────────────────────────────── */

#include <stdint.h>
#include <string.h>
#include "splash.h"

/* screen_width is set at startup in main.c */
extern int screen_width;

#ifdef __CC65__
#include <conio.h>

/* ── 6-wide × 7-tall bitmap font (bits 7-2 = columns 0-5, MSB = left) ─── */
/* Each byte: bit7=col0, bit6=col1, bit5=col2, bit4=col3, bit3=col4, bit2=col5 */

#define GW 6   /* glyph width in character cells  */
#define GH 7   /* glyph height in character cells  */

static const uint8_t g_F[GH] = {
    0xFC, /* ######  */
    0xC0, /* ##....  */
    0xC0, /* ##....  */
    0xF8, /* #####.  */
    0xC0, /* ##....  */
    0xC0, /* ##....  */
    0xC0  /* ##....  */
};
static const uint8_t g_U[GH] = {
    0xCC, /* ##.##.  */
    0xCC, /* ##.##.  */
    0xCC, /* ##.##.  */
    0xCC, /* ##.##.  */
    0xCC, /* ##.##.  */
    0xCC, /* ##.##.  */
    0x78  /* .####.  */
};
static const uint8_t g_J[GH] = {
    0x3C, /* ..####  */
    0x0C, /* ....##  */
    0x0C, /* ....##  */
    0x0C, /* ....##  */
    0x0C, /* ....##  */
    0xCC, /* ##..##  */
    0x78  /* .####.  */
};
static const uint8_t g_I[GH] = {
    0xFC, /* ######  */
    0x30, /* ..##..  */
    0x30, /* ..##..  */
    0x30, /* ..##..  */
    0x30, /* ..##..  */
    0x30, /* ..##..  */
    0xFC  /* ######  */
};
static const uint8_t g_B[GH] = {
    0xF8, /* #####.  */
    0xCC, /* ##.##.  */
    0xCC, /* ##.##.  */
    0xF8, /* #####.  */
    0xCC, /* ##.##.  */
    0xCC, /* ##.##.  */
    0xF8  /* #####.  */
};
static const uint8_t g_L[GH] = {
    0xC0, /* ##....  */
    0xC0, /* ##....  */
    0xC0, /* ##....  */
    0xC0, /* ##....  */
    0xC0, /* ##....  */
    0xC0, /* ##....  */
    0xFC  /* ######  */
};
static const uint8_t g_O[GH] = {
    0x78, /* .####.  */
    0xCC, /* ##.##.  */
    0xCC, /* ##.##.  */
    0xCC, /* ##.##.  */
    0xCC, /* ##.##.  */
    0xCC, /* ##.##.  */
    0x78  /* .####.  */
};
static const uint8_t g_G[GH] = {
    0x78, /* .####.  */
    0xC0, /* ##....  */
    0xC0, /* ##....  */
    0xD8, /* ##.##.  */
    0xCC, /* ##.##.  */
    0xCC, /* ##.##.  */
    0x7C  /* .#####  */
};
static const uint8_t g_E[GH] = {
    0xFC, /* ######  */
    0xC0, /* ##....  */
    0xC0, /* ##....  */
    0xF8, /* #####.  */
    0xC0, /* ##....  */
    0xC0, /* ##....  */
    0xFC  /* ######  */
};
static const uint8_t g_R[GH] = {
    0xF8, /* #####.  */
    0xCC, /* ##.##.  */
    0xCC, /* ##.##.  */
    0xF8, /* #####.  */
    0xE0, /* ###...  */
    0xD0, /* ##.#..  */
    0xCC  /* ##.##.  */
};

/* ── Draw one glyph at (ox, oy) using inverse-space filled pixels ─────── */
static void draw_glyph(const uint8_t *g, uint8_t ox, uint8_t oy)
{
    uint8_t row, col, rev = 0;
    for (row = 0; row < GH; row++) {
        uint8_t bits = g[row];
        gotoxy(ox, (uint8_t)(oy + row));
        for (col = 0; col < GW; col++) {
            uint8_t want = (bits & (0x80u >> col)) ? 1u : 0u;
            if (want != rev) { revers(want); rev = want; }
            cputc(' ');
        }
    }
    if (rev) revers(0);
}

/* ── Helpers ─────────────────────────────────────────────────────────── */
static void hline(uint8_t x, uint8_t y, uint8_t len, uint8_t inv)
{
    uint8_t i;
    gotoxy(x, y);
    revers(inv);
    for (i = 0; i < len; i++) cputc(' ');
    revers(0);
}

static void center_text(const char *s, uint8_t y, uint8_t cols)
{
    uint8_t len = (uint8_t)strlen(s);
    uint8_t x   = (cols > len) ? (cols - len) / 2 : 0;
    gotoxy(x, y);
    cputs(s);
}

/* ── Signal arcs (text art, top-left) ────────────────────────────────── */
static void draw_arcs(uint8_t cols)
{
    /* Three concentric arc outlines using standard characters.
       Positioned at left for 80-col, centered for 40-col.       */
    uint8_t ox = (cols >= 80) ? 2 : 0;

    gotoxy((uint8_t)(ox + 4), 0); cputc('.');
    gotoxy((uint8_t)(ox + 0), 1); cputc('.');
    gotoxy((uint8_t)(ox + 3), 1); cputc('.');
    gotoxy((uint8_t)(ox + 6), 1); cputc('.');
    gotoxy((uint8_t)(ox + 9), 1); cputc('.');
    gotoxy((uint8_t)(ox + 1), 2); cputc('.');
    gotoxy((uint8_t)(ox + 4), 2); cputc('*');  /* origin */
    gotoxy((uint8_t)(ox + 7), 2); cputc('.');
    gotoxy((uint8_t)(ox + 2), 3); cputc('.');
    gotoxy((uint8_t)(ox + 5), 3); cputc('.');
    gotoxy((uint8_t)(ox + 8), 3); cputc('.');
}

/* ── Public entry point ──────────────────────────────────────────────── */
void show_splash_hires(void)
{
    uint8_t cols  = (uint8_t)screen_width;
    uint8_t fuji_x, blog_x;

    /* "FUJI": 4 glyphs × 6 + 3 gaps × 1 = 27 wide */
    fuji_x = (cols > 27) ? (uint8_t)((cols - 27) / 2) : 0;
    /* "BLOGGER": 7 glyphs × 6 + 6 gaps × 1 = 48 wide */
    blog_x = (cols > 48) ? (uint8_t)((cols - 48) / 2) : 0;

    clrscr();

    /* Signal arcs top-left */
    draw_arcs(cols);

    /* FUJI — rows 4-10 */
    draw_glyph(g_F, (uint8_t)(fuji_x),      4);
    draw_glyph(g_U, (uint8_t)(fuji_x +  7), 4);
    draw_glyph(g_J, (uint8_t)(fuji_x + 14), 4);
    draw_glyph(g_I, (uint8_t)(fuji_x + 21), 4);

    /* BLOGGER — rows 12-18 */
    draw_glyph(g_B, (uint8_t)(blog_x),      12);
    draw_glyph(g_L, (uint8_t)(blog_x +  7), 12);
    draw_glyph(g_O, (uint8_t)(blog_x + 14), 12);
    draw_glyph(g_G, (uint8_t)(blog_x + 21), 12);
    draw_glyph(g_G, (uint8_t)(blog_x + 28), 12);
    draw_glyph(g_E, (uint8_t)(blog_x + 35), 12);
    draw_glyph(g_R, (uint8_t)(blog_x + 42), 12);

    /* Double divider row 19: one inverse (solid), one normal dashes */
    hline(0, 19, cols, 1);
    gotoxy(0, 20); revers(0);
    {
        uint8_t i;
        for (i = 0; i < cols; i++) cputc('-');
    }
    /* Cursor is now at row 21 — show_splash() writes init info there */
}

#else   /* non-CC65 dev build: simple text fallback */

#include <stdio.h>
void show_splash_hires(void)
{
    printf("\n  === FUJIBLOGGER ===\n");
    printf("  WRITE ON THE WIRE\n\n");
}

#endif  /* __CC65__ */
