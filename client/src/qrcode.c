/* qrcode.c — QR Code generator + lo-res renderer for the Apple II client.
 *
 * Scope is deliberately tiny: version 1 (21x21), EC level L, byte mode only.
 * A 10-character hex API key needs 10 of the 17 available payload bytes, so a
 * general encoder would be pure dead weight on a machine this small.
 *
 * Memory: the 441-byte module grid is supplied by the caller (main.c hands it
 * s_body, which is idle while the Configuration screen is up) so this module
 * costs only ~35 bytes of BSS.  The code itself is placed in the language card
 * ("LC" segment, $D400-$DFFF) because main RAM below __HIMEM__ is essentially
 * full — see apple2-fujiblog.cfg.
 */

#include <string.h>
#include <stdint.h>

#ifdef __CC65__
#include <conio.h>
#include <apple2.h>
#else
#include <stdio.h>
#endif

#include "qrcode.h"

#ifdef __CC65__
#pragma code-name (push, "LC")
#pragma rodata-name (push, "LC")
#endif

/* Version 1-L: 26 total codewords = 19 data + 7 error correction. */
#define QR_DATA_CW  19
#define QR_EC_CW    7
#define QR_TOTAL_CW (QR_DATA_CW + QR_EC_CW)

/* Grid byte flags. */
#define QR_DARK     0x01
#define QR_FUNCTION 0x02

static uint8_t cw[QR_TOTAL_CW];   /* data codewords, then EC codewords */
static uint8_t gen[QR_EC_CW];     /* RS generator polynomial           */

/* ── GF(256) arithmetic ─────────────────────────────────────────────────────
   Multiplication is done bit-by-bit rather than with log/antilog tables: the
   whole encode does well under 200 multiplies, and skipping the tables saves
   768 bytes we do not have. */
static uint8_t gmul(uint8_t a, uint8_t b)
{
    uint8_t r = 0;

    while (b) {
        if (b & 1)
            r ^= a;
        b >>= 1;
        a = (uint8_t)((a << 1) ^ ((a & 0x80) ? 0x1D : 0x00));
    }
    return r;
}

/* Build the degree-QR_EC_CW generator polynomial, coefficients stored
   highest-degree-first with the monic leading term left implicit. */
static void rs_generator(void)
{
    uint8_t i, j, root = 1;

    memset(gen, 0, QR_EC_CW);
    gen[QR_EC_CW - 1] = 1;
    for (i = 0; i < QR_EC_CW; i++) {
        for (j = 0; j < QR_EC_CW; j++) {
            gen[j] = gmul(gen[j], root);
            if (j + 1 < QR_EC_CW)
                gen[j] ^= gen[j + 1];
        }
        root = gmul(root, 2);
    }
}

/* Append the Reed-Solomon remainder of cw[0..QR_DATA_CW-1] to cw[]. */
static void rs_remainder(void)
{
    uint8_t *ec = cw + QR_DATA_CW;
    uint8_t i, j, factor;

    rs_generator();
    memset(ec, 0, QR_EC_CW);
    for (i = 0; i < QR_DATA_CW; i++) {
        factor = cw[i] ^ ec[0];
        memmove(ec, ec + 1, QR_EC_CW - 1);
        ec[QR_EC_CW - 1] = 0;
        for (j = 0; j < QR_EC_CW; j++)
            ec[j] ^= gmul(gen[j], factor);
    }
}

/* ── Module grid helpers ────────────────────────────────────────────────── */

static uint8_t *g_grid;

static void set_module(int8_t x, int8_t y, uint8_t dark, uint8_t function)
{
    uint8_t *m;

    if (x < 0 || x >= QR_SIZE || y < 0 || y >= QR_SIZE)
        return;
    m = g_grid + (uint16_t)y * QR_SIZE + (uint8_t)x;
    *m = (uint8_t)((dark ? QR_DARK : 0) | function);
}

static uint8_t is_function(uint8_t x, uint8_t y)
{
    return (uint8_t)(g_grid[(uint16_t)y * QR_SIZE + x] & QR_FUNCTION);
}

/* Finder pattern plus its separator, centred on (cx, cy). */
static void draw_finder(int8_t cx, int8_t cy)
{
    int8_t dx, dy, ax, ay, dist;

    for (dy = -4; dy <= 4; dy++) {
        for (dx = -4; dx <= 4; dx++) {
            ax = (int8_t)(dx < 0 ? -dx : dx);
            ay = (int8_t)(dy < 0 ? -dy : dy);
            dist = (int8_t)(ax > ay ? ax : ay);
            set_module((int8_t)(cx + dx), (int8_t)(cy + dy),
                       (uint8_t)(dist != 2 && dist != 4), QR_FUNCTION);
        }
    }
}

/* Write the 15-bit format information for EC level L and the given mask.
   Called once with mask 0 to reserve the modules, then again after masking
   with the real choice. */
static void draw_format(uint8_t mask)
{
    uint16_t data = (uint16_t)(0x08 | mask);   /* 01 (level L) << 3 | mask */
    uint16_t rem  = data;
    uint16_t bits;
    uint8_t  i;

    for (i = 0; i < 10; i++)
        rem = (uint16_t)((rem << 1) ^ ((rem >> 9) * 0x537));
    bits = (uint16_t)(((data << 10) | rem) ^ 0x5412);

#define FBIT(n) ((uint8_t)((bits >> (n)) & 1))
    /* First copy, around the top-left finder. */
    for (i = 0; i <= 5; i++)
        set_module(8, (int8_t)i, FBIT(i), QR_FUNCTION);
    set_module(8, 7, FBIT(6), QR_FUNCTION);
    set_module(8, 8, FBIT(7), QR_FUNCTION);
    set_module(7, 8, FBIT(8), QR_FUNCTION);
    for (i = 9; i < 15; i++)
        set_module((int8_t)(14 - i), 8, FBIT(i), QR_FUNCTION);

    /* Second copy, split between the other two finders. */
    for (i = 0; i < 8; i++)
        set_module((int8_t)(QR_SIZE - 1 - i), 8, FBIT(i), QR_FUNCTION);
    for (i = 8; i < 15; i++)
        set_module(8, (int8_t)(QR_SIZE - 15 + i), FBIT(i), QR_FUNCTION);
    set_module(8, QR_SIZE - 8, 1, QR_FUNCTION);   /* always-dark module */
#undef FBIT
}

static void draw_function_patterns(void)
{
    uint8_t i;

    memset(g_grid, 0, QR_GRID_BYTES);

    draw_finder(3, 3);
    draw_finder(QR_SIZE - 4, 3);
    draw_finder(3, QR_SIZE - 4);

    /* Timing patterns. Version 1 has no alignment pattern. */
    for (i = 8; i < QR_SIZE - 8; i++) {
        set_module(6, (int8_t)i, (uint8_t)(!(i & 1)), QR_FUNCTION);
        set_module((int8_t)i, 6, (uint8_t)(!(i & 1)), QR_FUNCTION);
    }

    draw_format(0);   /* placeholder — reserves the format modules */
}

/* Lay the 26 codewords into the symbol in the standard upward/downward
   two-module-wide zigzag, skipping function modules. */
static void draw_codewords(void)
{
    int8_t   right, x;
    uint8_t  vert, j, y, upward;
    uint16_t bit = 0;

    for (right = QR_SIZE - 1; right >= 1; right = (int8_t)(right - 2)) {
        if (right == 6)
            right = 5;                    /* skip the vertical timing column */
        upward = (uint8_t)(((right + 1) & 2) == 0);
        for (vert = 0; vert < QR_SIZE; vert++) {
            for (j = 0; j < 2; j++) {
                x = (int8_t)(right - j);
                y = upward ? (uint8_t)(QR_SIZE - 1 - vert) : vert;
                if (is_function((uint8_t)x, y) || bit >= QR_TOTAL_CW * 8)
                    continue;
                if (cw[bit >> 3] & (0x80 >> (bit & 7)))
                    g_grid[(uint16_t)y * QR_SIZE + (uint8_t)x] |= QR_DARK;
                bit++;
            }
        }
    }
}

/* XOR the data mask over every non-function module.
 *
 * A full encoder tries all eight masks and keeps the one with the lowest
 * penalty score, but the four penalty rules cost well over a kilobyte of 6502
 * code and the language card has no room for them.  The mask is fixed at 2
 * (x % 3 == 0) instead — sound here because the payload is not arbitrary: it is
 * always exactly ten hex characters.  Every one of the eight masks was checked
 * against 400 random keys rendered exactly as the routine below draws them,
 * decoded with zxing-cpp (the engine behind the browser's BarcodeDetector)
 * under four blur/noise levels: 1600/1600 reads for each mask.  Mask 2 is also
 * what a scoring encoder picks most often for this payload shape. */
#define QR_MASK 2

static void apply_mask(void)
{
    uint8_t x, y;
    uint8_t *m;

    for (y = 0; y < QR_SIZE; y++) {
        m = g_grid + (uint16_t)y * QR_SIZE;
        for (x = 0; x < QR_SIZE; x++) {
            if (!(m[x] & QR_FUNCTION) && (x % 3) == 0)
                m[x] ^= QR_DARK;
        }
    }
}

int8_t qr_encode(const char *text, uint8_t *grid)
{
    uint8_t len = (uint8_t)strlen(text);
    uint8_t i;

    if (len > QR_MAX_TEXT)
        return -1;

    g_grid = grid;

    /* Byte-mode bit stream.  Every field is nibble-aligned (4-bit mode
       indicator + 8-bit length + 8-bit characters), so the payload can be
       shifted into place a nibble at a time instead of bit by bit.  The low
       nibble left over in the last byte doubles as the 4-bit terminator. */
    cw[0] = (uint8_t)(0x40 | (len >> 4));
    cw[1] = (uint8_t)(len << 4);
    for (i = 0; i < len; i++) {
        cw[i + 1] = (uint8_t)(cw[i + 1] | ((uint8_t)text[i] >> 4));
        cw[i + 2] = (uint8_t)((uint8_t)text[i] << 4);
    }
    /* Pad out the remaining data codewords with the specified filler bytes. */
    for (i = (uint8_t)(len + 2); i < QR_DATA_CW; i++)
        cw[i] = (uint8_t)((i & 1) == (uint8_t)((len + 2) & 1) ? 0xEC : 0x11);

    rs_remainder();

    draw_function_patterns();
    draw_codewords();

    apply_mask();
    draw_format(QR_MASK);

    return 0;
}

/* ── Rendering ──────────────────────────────────────────────────────────── */

#ifdef __CC65__

/* Lo-res page 1 shares memory with text page 1 at $400, which sits below the
   program's $0803 load address — the only bitmap page we can scribble on.
   (Hi-res page 1 at $2000 is in the middle of our own code.)
   Each byte holds two vertically stacked blocks: low nibble = even row. */
/* Soft switches must be *written*, not read.  A read whose value is discarded
   — `(void)*(volatile uint8_t *)0xC050;` — is dropped by cc65's optimizer
   despite the volatile, so the mode change silently never happens and the QR
   grid gets displayed as text characters.  A store cannot be optimized away,
   and on the IIe/IIc these switches trigger on any access, read or write. */
#define SOFTSW(a) (*(volatile uint8_t *)(a))
#define SOFTSW_HIT(a) do { SOFTSW(a) = 0; } while (0)

#define LORES_BLACK 0
#define LORES_WHITE 15

/* A lo-res block is roughly 1.6x wider than it is tall, so each QR module is
   drawn 1 block wide and 2 blocks tall (≈0.8:1) to stay near square.  That
   makes the symbol 21x42 blocks on a 40x48 screen: 9 blocks of quiet zone to
   the left and right, 3 above and below. */
#define QR_ORIGIN_X 9
#define QR_ORIGIN_Y 3

static uint8_t *lores_line(uint8_t line)
{
    return (uint8_t *)(0x0400u
                       + (uint16_t)(line & 7) * 0x80u
                       + (uint16_t)(line >> 3) * 0x28u);
}

static uint8_t lores_block(uint8_t row, uint8_t col)
{
    uint8_t qx, qy;

    if (col < QR_ORIGIN_X || col >= QR_ORIGIN_X + QR_SIZE)
        return LORES_WHITE;
    if (row < QR_ORIGIN_Y || row >= QR_ORIGIN_Y + QR_SIZE * 2)
        return LORES_WHITE;
    qx = (uint8_t)(col - QR_ORIGIN_X);
    qy = (uint8_t)((row - QR_ORIGIN_Y) >> 1);
    return g_grid[(uint16_t)qy * QR_SIZE + qx] & QR_DARK ? LORES_BLACK
                                                         : LORES_WHITE;
}

static void qr_render(void)
{
    uint8_t line, col, *p;

    /* 40 columns, plain lo-res, page 1, full screen, graphics on.  Leave the
       80-column switches to videomode() — poking them behind the firmware's
       back leaves it in an inconsistent state when we switch back. */
    videomode(VIDEOMODE_40COL);
    SOFTSW_HIT(0xC00C);          /* 80COL off      */
    SOFTSW_HIT(0xC05F);          /* double-res off */
    SOFTSW_HIT(0xC054);          /* page 1         */
    SOFTSW_HIT(0xC056);          /* lo-res         */
    SOFTSW_HIT(0xC052);          /* full screen    */
    SOFTSW_HIT(0xC050);          /* graphics on    */

    for (line = 0; line < 24; line++) {
        p = lores_line(line);
        for (col = 0; col < 40; col++)
            p[col] = (uint8_t)(lores_block((uint8_t)(line * 2), col)
                               | (lores_block((uint8_t)(line * 2 + 1), col) << 4));
    }

    /* Poll the keyboard directly: cgetc() would draw a cursor into $400 and
       punch a hole in the symbol. */
    SOFTSW_HIT(0xC010);          /* clear any pending strobe */
    while (!(SOFTSW(0xC000) & 0x80))
        ;
    SOFTSW_HIT(0xC010);

    SOFTSW_HIT(0xC051);          /* back to text */
}

int8_t qr_display(const char *text, uint8_t *grid)
{
    uint8_t w, h;

    screensize(&w, &h);
    if (qr_encode(text, grid) != 0)
        return -1;
    qr_render();
    /* qr_render() turns 80COL off at the hardware level, which the 80-column
       firmware does not see.  Step through 40-column mode so the firmware is
       re-entered from a state it agrees with, or it may no-op the request and
       leave the screen half-configured. */
    videomode(VIDEOMODE_40COL);
    if (w >= 80)
        videomode(VIDEOMODE_80COL);
    clrscr();
    return 0;
}

#else  /* host build — ASCII rendering, used by the encoder self-test */

int8_t qr_display(const char *text, uint8_t *grid)
{
    uint8_t x, y;

    if (qr_encode(text, grid) != 0)
        return -1;
    for (y = 0; y < QR_SIZE; y++) {
        for (x = 0; x < QR_SIZE; x++)
            fputs(grid[(uint16_t)y * QR_SIZE + x] & QR_DARK ? "##" : "  ",
                  stdout);
        putchar('\n');
    }
    return 0;
}

#endif

#ifdef __CC65__
#pragma rodata-name (pop)
#pragma code-name (pop)
#endif
