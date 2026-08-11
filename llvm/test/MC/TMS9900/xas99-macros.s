; RUN: llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
; RUN:   -filetype=obj %s -o %t.o
; RUN: llvm-readobj --hex-dump=.text %t.o | FileCheck %s

        .DEFM LOADPAIR
        LI #1,#2
        MOV #1,#3
        .ENDM

        .DEFM TWICE
        .LOADPAIR #1,#2,#3
        INCT #1
        .ENDM

        .DEFM LITERAL
        TEXT '#1'
        .ENDM

        .DEFM CLEAR
        CLR #1
        .ENDM

        .twice R4,>1234,R5
        .LITERAL 7
        .CLEAR    *R1
        .CLEAR R2    * trailing comment

; CHECK:      Hex dump of section '.text':
; CHECK-NEXT: 0x00000000 02041234 c14405c4 233104d1 04c2
