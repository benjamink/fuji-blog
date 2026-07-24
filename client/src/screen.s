;
; screen.s — one-row screen-memory scroll for the body editor.
;
; Shifts the editor's edit area up or down by exactly one text row by moving the
; character cells already on screen (text page 1, base $0400).  In 80-column
; mode the odd columns live in auxiliary memory, reached through the 80STORE /
; PAGE2 soft switches; the main and aux halves are moved as two passes.  Because
; the cells are already rendered, the C caller only redraws the row(s) that
; scroll into view — a full-screen repaint becomes one memory move plus a row.
;
; Placed in the "LC" segment so it runs from the language card ($D400), like
; qrcode.c, because main RAM below __HIMEM__ is full.  It calls nothing (no
; fujinet-lib, no ROM), so executing from the LC bank is safe.
;
; C entry (see screen.h):
;   void __fastcall__ scr_scroll(unsigned char arg);   ; arg arrives in A
;     bits 0-4 : edit-area row count (>= 2)              -> pass `er`
;     bit 5    : 80-column, also move aux (SCR_W80  $20)
;     bit 6    : scroll DOWN, new row at top (SCR_DOWN $40); clear = UP
;   The edit area starts at screen row 2 (== WP_HDR in main.c).
;

        .export _scr_scroll
        .importzp ptr1, ptr2, tmp1, tmp2, tmp3, tmp4

WE_TOP   = 2            ; edit-area top screen row (keep in sync with WP_HDR)
ROWBYTES = 40          ; bytes per text row within one memory bank
SCR_W80  = $20
SCR_DOWN = $40

STORE80OFF = $C000     ; write: 80STORE off
STORE80ON  = $C001     ; write: 80STORE on
TXTPAGE1   = $C054     ; access: PAGE2 off -> $400-7FF = main
TXTPAGE2   = $C055     ; access: PAGE2 on  -> $400-7FF = aux
RD80STORE  = $C018     ; read bit7 = 80STORE state
RDPAGE2    = $C01C     ; read bit7 = PAGE2 state

        .segment "LC"

; Start address of each text row's 40 bytes: $0400 + (Y&7)*$80 + (Y>>3)*$28.
RBASEL:
        .byte $00,$80,$00,$80,$00,$80,$00,$80   ; rows 0-7
        .byte $28,$A8,$28,$A8,$28,$A8,$28,$A8   ; rows 8-15
        .byte $50,$D0,$50,$D0,$50,$D0,$50,$D0   ; rows 16-23
RBASEH:
        .byte $04,$04,$05,$05,$06,$06,$07,$07   ; rows 0-7
        .byte $04,$04,$05,$05,$06,$06,$07,$07   ; rows 8-15
        .byte $04,$04,$05,$05,$06,$06,$07,$07   ; rows 16-23

.proc _scr_scroll
        pha                     ; A = arg
        and #$1F
        sta tmp1                ; tmp1 = rows
        pla
        sta tmp2                ; tmp2 = flags

        lda tmp1
        cmp #2
        bcc done                ; fewer than 2 rows: nothing to scroll

        ; 80-column: save current banking state and force 80STORE on.
        lda tmp2
        and #SCR_W80
        beq nobank
        lda RD80STORE
        sta tmp3                ; saved 80STORE (bit7)
        lda RDPAGE2
        sta tmp4                ; saved PAGE2  (bit7)
        sta STORE80ON           ; access triggers; written value ignored
nobank:

        ; lim = WE_TOP + rows - 1  (bottom edit row); reuse tmp1
        lda #WE_TOP
        clc
        adc tmp1
        sec
        sbc #1
        sta tmp1                ; tmp1 = lim

        lda tmp2
        and #SCR_DOWN
        bne down

; UP: content moves up.  dst = WE_TOP..lim-1, src = dst+1.
        ldx #WE_TOP
uploop:
        cpx tmp1
        bcs finish              ; X >= lim -> done
        jsr setdst              ; ptr2 = base[X]
        txa
        clc
        adc #1                  ; A = src = dst+1
        jsr setsrc              ; ptr1 = base[src]
        jsr moverow
        inx
        jmp uploop

; DOWN: content moves down.  dst = lim..WE_TOP+1, src = dst-1.
down:
        ldx tmp1                ; X = lim
dnloop:
        cpx #WE_TOP+1
        bcc finish              ; X < WE_TOP+1 -> done
        jsr setdst              ; ptr2 = base[X]
        txa
        sec
        sbc #1                  ; A = src = dst-1
        jsr setsrc              ; ptr1 = base[src]
        jsr moverow
        dex
        jmp dnloop

finish:
        lda tmp2
        and #SCR_W80
        beq done                ; 40-col: never touched the switches
        bit tmp3                ; restore 80STORE
        bpl s80off
        sta STORE80ON
        jmp rpage2
s80off:
        sta STORE80OFF
rpage2:
        bit tmp4                ; restore PAGE2
        bpl p2main
        sta TXTPAGE2
        jmp done
p2main:
        sta TXTPAGE1
done:
        rts

; ptr2 = RBASE[X]
setdst:
        lda RBASEL,x
        sta ptr2
        lda RBASEH,x
        sta ptr2+1
        rts

; ptr1 = RBASE[A]   (clobbers Y; moverow resets it)
setsrc:
        tay
        lda RBASEL,y
        sta ptr1
        lda RBASEH,y
        sta ptr1+1
        rts

; Copy one row (ptr1)->(ptr2): main bank, then aux bank if 80-column.
moverow:
        lda tmp2
        and #SCR_W80
        beq copyrow             ; 40-col: single bank, switches untouched
        sta TXTPAGE1            ; select main for $400-7FF
        jsr copyrow
        sta TXTPAGE2            ; select aux for $400-7FF
        jmp copyrow             ; tail call: its rts returns to moverow's caller

copyrow:
        ldy #ROWBYTES-1
cr1:    lda (ptr1),y
        sta (ptr2),y
        dey
        bpl cr1
        rts
.endproc
