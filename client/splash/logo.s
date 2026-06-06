; logo.s — embed the pre-rendered HGR image and jump to code at $4000.
;
; Layout (follows FujiNet's fujinet-config approach):
;   $2000-$3FFF  HGR segment: 8192 bytes of logo.hgr pixel data
;   $2000-$2002  JMP segment overwrite: "JMP $4000" (3 bytes)
;   $4000+       STARTUP/CODE segments: cc65 startup + C code
;
; When ProDOS loads this SYS file to $2000 and executes:
;   1. $2000 = JMP $4000  (first 3 bytes of HGR data overwritten)
;   2. cc65 startup at $4000 runs (ALLOC_INTERRUPT works — no loader conflict)
;   3. main() displays the HGR image and waits
;   4. exit(0) → ProDOS QUIT → chains to FUJIBLOG.SYSTEM

        .segment "HGR"
        .incbin "logo.hgr"      ; 8192 bytes of pixel data at $2000

        .segment "JMP"
        jmp     $4000            ; overwrite first 3 bytes of HGR data
