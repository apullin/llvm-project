; RUN: llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
; RUN:   -filetype=obj %s -o %t.o
; RUN: llvm-readobj --hex-dump=.text %t.o | FileCheck %s

va      EQU 7
vr      REQU R15
vb      REQU 2
zero    EQU 0
base    EQU >20

        DATA R1,R15,va,vr,vb
        CLR vr
        LI VR,va+zero
        CLR va
        INC @va(vr)
        DATA base->4

; CHECK:      Hex dump of section '.text':
; CHECK-NEXT: 0x00000000 0001000f 0007000f 000204cf 020f0007
; CHECK-NEXT: 0x00000010 04c705af 0007001c
