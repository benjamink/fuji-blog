/* splash_main.c — Minimal HGR splash for FujiBlogger.
   No conio, no C library beyond exit().  Pure hardware I/O.
   Text ("FUJIBLOGGER V<ver>" / "PRESS ANY KEY...") is baked into the HGR
   image by gen_hgr.py.  crt0 exports _exit, which chains to FUJIBLOG.SYSTEM. */

#include <stdlib.h>   /* exit() — satisfied by _exit in splash_crt0.s */

#define SW(a)  (*(volatile unsigned char *)(unsigned int)(a))

int main(void)
{
    /* Clear the 3-byte JMP stub at $2000 so it doesn't show as noise. */
    *(unsigned char *)0x2000 = 0x00;
    *(unsigned char *)0x2001 = 0x00;
    *(unsigned char *)0x2002 = 0x00;

    /* Enable HGR page 1, full-screen (text is part of the HGR image). */
    SW(0xC057) = 0;   /* HIRES  */
    SW(0xC052) = 0;   /* FULL   (not mixed) */
    SW(0xC050) = 0;   /* GRAPH  */

    /* Wait for keypress or ~4-second timeout. */
    {
        unsigned char a = 0, b = 0, c = 0;
        for (;;) {
            if (SW(0xC000) & 0x80) {
                SW(0xC010);   /* clear keyboard strobe */
                break;
            }
            ++c;
            if (!c) { ++b; if (!b) { ++a; if (a == 4) break; } }
        }
    }

    SW(0xC051) = 0;   /* back to TEXT mode */
    exit(0);          /* crt0._exit chains to FUJIBLOG.SYSTEM */
    return 0;
}
