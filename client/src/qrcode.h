/* qrcode.h — minimal QR Code generator for the Apple II client.
 *
 * Fixed to QR version 1 (21x21 modules), error-correction level L, byte mode.
 * That is all the API key handoff needs: a 10-character hex key is 10 bytes,
 * well inside version 1-L's 17-byte payload budget.  Restricting the encoder to
 * one version keeps the code small enough to live in the language card (see the
 * "LC" pragmas in qrcode.c) instead of eating scarce main RAM.
 */
#ifndef QRCODE_H
#define QRCODE_H

#include <stdint.h>

#define QR_SIZE       21                  /* version 1: 21x21 modules       */
#define QR_QUIET      4                   /* mandatory quiet-zone margin    */
#define QR_MAX_TEXT   17                  /* version 1-L byte-mode capacity */
#define QR_GRID_BYTES (QR_SIZE * QR_SIZE)  /* 441 — caller-supplied scratch  */

/* Encode `text` into `grid` (at least QR_GRID_BYTES bytes, supplied by the
   caller so this module adds no BSS of its own).  On return, bit 0 of
   grid[y * QR_SIZE + x] is 1 for a dark module.  Returns 0 on success, -1 if
   `text` is longer than QR_MAX_TEXT. */
int8_t qr_encode(const char *text, uint8_t *grid);

/* Encode `text` and show it full-screen, then wait for a keypress.  `grid` is
   the same scratch buffer qr_encode() needs.  Returns 0 on success, -1 if the
   text does not fit. */
int8_t qr_display(const char *text, uint8_t *grid);

#endif /* QRCODE_H */
