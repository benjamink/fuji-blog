/* splash_main.c — HGR splash for FujiBlogger.

   The logo is pre-loaded in $2000-$3FFF (logo.hgr embedded at build time).
   We display it in MIXED mode (graphics on top, 4 text rows at the bottom)
   and print the version line + "PRESS ANY KEY..." as LIVE text written
   directly to the text page.  Writing the text at runtime (rather than baking
   it into the HGR image) means it always reflects the current VERSION.

   Pure hardware I/O only — no conio, no constructors/destructors — to keep the
   chain to FUJIBLOG.SYSTEM (in splash_crt0.s) rock solid.  */

#include <stdlib.h>   /* exit() — satisfied by _exit in splash_crt0.s */

#define SW(a)  (*(volatile unsigned char *)(unsigned int)(a))

#ifndef VERSION
#define VERSION "?"
#endif

/* Apple II 40-column text page 1 base address for screen row 0-23:
   $0400 + (row & 7) * $80 + (row >> 3) * 40.  Normal-video characters are
   stored with the high bit set, so we OR each byte with $80.               */
static unsigned char *text_row_addr(unsigned char row)
{
    unsigned int base = 0x0400u
                      + ((unsigned int)(row & 7u) << 7)
                      + (unsigned int)(row >> 3) * 40u;
    return (unsigned char *)base;
}

/* Fill the whole 40x24 text page 1 with blanks ($A0 = normal-video space). */
static void text_clear(void)
{
    unsigned char row, col;
    for (row = 0; row < 24; row++) {
        unsigned char *p = text_row_addr(row);
        for (col = 0; col < 40; col++)
            p[col] = 0xA0;
    }
}

/* Write str centred on 40-column text row `row`, normal video. */
static void text_center(unsigned char row, const char *str)
{
    unsigned char len = 0, x, i;
    unsigned char *p;
    while (str[len])
        len++;
    x = (len < 40) ? (unsigned char)((40 - len) / 2) : 0;
    p = text_row_addr(row);
    for (i = 0; str[i]; i++)
        p[x + i] = (unsigned char)(str[i] | 0x80);
}

int main(void)
{
    /* Clear the 3-byte JMP stub at $2000 so it doesn't show as noise. */
    *(unsigned char *)0x2000 = 0x00;
    *(unsigned char *)0x2001 = 0x00;
    *(unsigned char *)0x2002 = 0x00;

    /* Prepare the text page: blank it, then write the two live-text lines
       into the 4 MIXED text rows (screen rows 20-23). */
    text_clear();
    text_center(21, "FUJIBLOGGER V" VERSION);
    text_center(23, "PRESS ANY KEY...");

    /* Enable HGR page 1 in MIXED mode (graphics top, text rows 20-23). */
    SW(0xC057) = 0;   /* HIRES  */
    SW(0xC053) = 0;   /* MIXED  */
    SW(0xC050) = 0;   /* GRAPH  */

    /* Wait for a keypress, or auto-continue after ~3 seconds. */
    {
        unsigned char a = 0, b = 0, c = 0;
        for (;;) {
            if (SW(0xC000) & 0x80) {
                SW(0xC010);   /* clear keyboard strobe */
                break;
            }
            ++c;
            if (!c) { ++b; if (!b) { ++a; if (a == 3) break; } }
        }
    }

    /* Blank the text page again so the stale splash text (and the old ProDOS
       boot screen) don't flash when we switch back to full text mode. */
    text_clear();

    SW(0xC051) = 0;   /* back to TEXT mode (now blank) */
    exit(0);          /* crt0._exit chains to FUJIBLOG.SYSTEM */
    return 0;
}
