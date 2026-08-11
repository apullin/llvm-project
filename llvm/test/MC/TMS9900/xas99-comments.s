; RUN: llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
; RUN:   -filetype=obj %s -o %t.o
; RUN: llvm-readobj --hex-dump=.text %t.o | FileCheck %s

scale   EQU 3   * An xas99 comment field may itself contain * >0400.
        MOV R0,R1  * trailing instruction comment
        LI R2,2 * scale
        LI R3,2  * this star begins a comment instead
        B    *R11
        NOP    * zero-operand instruction comment
!       CLR R0  * anonymous local label before a register operand
        INC R1   free-form xas99 comment field
        RTWP    zero-operand free-form comment

; CHECK:      Hex dump of section '.text':
; CHECK-NEXT: 0x00000000 c0400202 00060203 0002045b 100004c0
; CHECK-NEXT: 0x00000010 05810380
