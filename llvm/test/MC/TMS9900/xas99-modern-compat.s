; RUN: split-file %s %t
; RUN: llvm-mc -triple=tms9900 -filetype=obj %t/gnu_hex_and_idx_branch.s -o %t/gnu.o
; RUN: llvm-objdump -d %t/gnu.o | FileCheck %s --check-prefix=GNU-DIS
; RUN: llvm-objdump -d -M xas99 %t/gnu.o | FileCheck %s --check-prefix=XAS-DIS
; RUN: llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 -filetype=obj %t/modern.asm -o %t/mod.o
; RUN: llvm-readobj --relocations --hex-dump=.text %t/mod.o | FileCheck %s --check-prefix=MOD-HEX

;--- gnu_hex_and_idx_branch.s
    .text
    LI   R0, >0080
    B    @0x1234(R1)
    BL   @0x5678(R2)
    B    *R3+
    BL   *R4+

; GNU-DIS:      LI R0,128
; GNU-DIS-NEXT: B @4660(R1)
; GNU-DIS-NEXT: BL @22136(R2)
; GNU-DIS-NEXT: B *R3+
; GNU-DIS-NEXT: BL *R4+

; XAS-DIS:      LI R0,>0080
; XAS-DIS-NEXT: B @>1234(R1)
; XAS-DIS-NEXT: BL @>5678(R2)
; XAS-DIS-NEXT: B *R3+
; XAS-DIS-NEXT: BL *R4+

;--- modern.asm
TITL 'MODERN'
IDT  'MODTEST'
* Sprite 0's apostrophe inside a '*' comment must not swallow lines!
       .equ VDPWD, 0x8C00

wrksp:
       equ  >8300
w1     wequ 1
w1     wequ 3

       rorg
msg:
       text 'HI'
       text '!!'

       dorg >A000
d_a    data 0
d_b    bes  4
       rorg

       lwpi wrksp
       li   r0, s#msg
       li   r1, d_b
       li   r2, >8000//2
       li   r3, 1 + 2 * 3
       movb b#>42, r4
       mov  w#>BEEF, r5
       b    @>6000(w1)
       auto
       end

; Expected bytes:
;   'HI!!'                  = 48 49 21 21
;   LWPI >8300              = 02e0 8300
;   LI R0, 4 (s#msg)        = 0200 0004
;   LI R1, >A006 (d_b)      = 0201 a006
;   LI R2, >4000 (>8000//2) = 0202 4000
;   LI R3, 9 ((1+2)*3 L->R) = 0203 0009
;   MOVB @auto_b, R4        = d120 0026
;   MOV  @auto_w, R5        = c160 0028
;   B    @>6000(R3)         = 0463 6000
;   auto_b: BYTE >42, pad   = 42 00
;   auto_w: DATA >BEEF      = beef
; MOD-HEX:      0x1A R_TMS9900_16 .text 0x24
; MOD-HEX-NEXT: 0x1E R_TMS9900_16 .text 0x26
; MOD-HEX:      Hex dump of section '.text':
; MOD-HEX-NEXT: 0x00000000 48492121 02e08300 02000004 0201a006
; MOD-HEX-NEXT: 0x00000010 02024000 02030009 d1200000 c1600000
; MOD-HEX-NEXT: 0x00000020 04636000 4200beef
