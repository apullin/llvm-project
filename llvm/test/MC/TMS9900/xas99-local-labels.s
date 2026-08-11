; RUN: llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
; RUN:   -filetype=obj %s -o %t.o
; RUN: llvm-readobj --hex-dump=.text %t.o | FileCheck %s

!loop   NOP
        JMP !done
        BYTE >AA
!done   JMP -!loop

!       BYTE 1
!       BYTE 2
        JMP -!!

        JMP !!
!       NOP
!       RT

; CHECK:      Hex dump of section '.text':
; CHECK-NEXT: 0x00000000 10001001 aa0010fc 010210fe 10011000
; CHECK-NEXT: 0x00000010 045b
