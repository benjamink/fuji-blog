; splash_crt0.s -- Minimal ProDOS startup for the HGR splash binary.
;
; After main() returns we:
;   1. GETPFX   – get current directory (e.g. /FUJIBLOG/)
;   2. Append   "FUJIBLOG.SYSTEM" to build a full ProDOS path
;   3. OPEN     the loader SYS file
;   4. READ     its 459 bytes directly into $2000 (overwriting our own code –
;               that is safe because we need nothing after the JMP)
;   5. CLOSE    the file handle
;   6. JMP $2000 – execute the cc65 loader which loads FUJIBLOG

        .export __STARTUP__ : absolute = 1
        .export _exit
        .import _main

        ; ProDOS I/O buffer: 1 KB, page-aligned, high in RAM clear of the
        ; HGR image ($2000-$3FFF), our code/BSS, and the ProDOS global page
        ; ($BF00+).  Fixed address avoids linker-alignment problems.
        io_buf = $B000

        ; ── Startup entry point ──────────────────────────────────────────
        .segment "STARTUP"

start:
        sei                     ; disable interrupts immediately

        ; cc65 software-stack pointer: grows down from $1BFF
        lda     #$FF
        sta     $00
        lda     #$1B
        sta     $01

        jsr     _main           ; run C main()

        ; ── _exit: chain to FUJIBLOG.SYSTEM ─────────────────────────────
_exit:
        cli                     ; re-enable interrupts for ProDOS calls

        ; GETPFX — fill path_buf with current ProDOS prefix
        jsr     $BF00
        .byte   $C7             ; GET_PREFIX
        .addr   getpfx_params

        ; Append "FUJIBLOG.SYSTEM" (15 chars) to the prefix
        ldx     path_buf        ; X = current prefix length
        ldy     #0
@copy:  lda     target_name,y
        sta     path_buf+1,x
        inx
        iny
        cpy     #15
        bne     @copy
        stx     path_buf        ; update length byte

        ; OPEN FUJIBLOG.SYSTEM
        jsr     $BF00
        .byte   $C8             ; OPEN
        .addr   open_params
        bne     open_failed     ; if OPEN failed, show error code

        ; Copy ref_num (written by OPEN at open_params+5) to READ+CLOSE blocks
        lda     open_params+5
        sta     read_params+1
        sta     close_params+1

        ; READ 512 bytes to $2000 (FUJIBLOG.SYSTEM is 459 bytes)
        jsr     $BF00
        .byte   $CA             ; READ
        .addr   read_params

        ; CLOSE the file handle
        jsr     $BF00
        .byte   $CC             ; CLOSE
        .addr   close_params

        ; ── Critical: rewrite the ProDOS pathname buffer at $0280 ───────
        ; ProDOS placed "SPLASH.SYSTEM" there when it launched us.  The cc65
        ; loader.system reads $0280 to learn its own name, strips ".SYSTEM",
        ; and loads the matching BIN.  We must overwrite it with the
        ; FUJIBLOG.SYSTEM path or it will look for a non-existent "SPLASH" BIN
        ; and fall through to Bitsy Bye.  path_buf already holds the full
        ; length-prefixed path "/.../FUJIBLOG.SYSTEM".
        ldx     path_buf        ; length byte
        ldy     #0
@p280:  lda     path_buf,y
        sta     $0280,y
        iny
        dex
        bpl     @p280           ; copy length byte + all path chars

        ; Jump directly into the freshly loaded cc65 loader
        jmp     $2000

        ; OPEN failed — show the error code on the text screen and halt so
        ; the failure is diagnosable instead of silently dropping to Bitsy Bye.
open_failed:
        pha                     ; save ProDOS error code
        bit     $C051           ; TEXT mode
        bit     $C054           ; PAGE 1
        jsr     $FC58           ; HOME (clear screen)
        ldy     #0
@msg:   lda     openerr_msg,y
        beq     @num
        ora     #$80            ; high bit set so COUT prints normal text
        jsr     $FDED           ; COUT
        iny
        bne     @msg
@num:   pla
        jsr     $FDDA           ; PRBYTE — print A as two hex digits
        jsr     $FD8E           ; CROUT
hang:   jmp     hang

        ; ── Data (writable — ProDOS writes ref_num into open_params) ─────
        .segment "DATA"

        ; GETPFX parameter block
getpfx_params:
        .byte   1
        .word   path_buf

        ; OPEN parameter block  (param count = 3, ref_num at offset +5)
open_params:
        .byte   3               ; param count
        .word   path_buf        ; pathname address
        .word   io_buf          ; 1 KB I/O buffer (page-aligned)
        .byte   0               ; ref_num OUTPUT — written by ProDOS

        ; READ parameter block
read_params:
        .byte   4               ; param count
        .byte   0               ; ref_num (filled in above)
        .word   $2000           ; destination — overwrite our own code!
        .word   $0200           ; request_count = 512 bytes (file is 459)
        .word   0               ; transfer_count OUTPUT

        ; CLOSE parameter block
close_params:
        .byte   1               ; param count
        .byte   0               ; ref_num (filled in above)

        ; ProDOS path string buffer (length byte + path chars)
path_buf:
        .res    65, 0

        ; ── RODATA ───────────────────────────────────────────────────────
        .segment "RODATA"

target_name:
        .byte   "FUJIBLOG.SYSTEM"   ; 15 chars, no null terminator

openerr_msg:
        .byte   "OPEN ERR=", $00    ; null-terminated diagnostic label
