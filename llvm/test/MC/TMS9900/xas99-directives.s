; RUN: llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
; RUN:   -filetype=obj %s -o %t.o
; RUN: llvm-readobj --sections --symbols --hex-dump=.text %t.o \
; RUN:   | FileCheck %s

        DEF start,aligned,entry
        AORG >0004
value   EQU >1234
start   DATA value,VALUE
        BYTE 'A'
        DATA 'AB'
        TEXT 'C',"D",''
odd     BYTE >99
aligned DATA >5678
code    BYTE >55
entry   LI R1,>08
length  EQU $-start
        DATA length,aligned-start,entry-start
        END START
        DATA >DEAD

; CHECK:      Name: .text
; CHECK:      Size: 30
; CHECK:      Name: START
; CHECK-NEXT: Value: 0x4
; CHECK:      Name: ALIGNED
; CHECK-NEXT: Value: 0x10
; CHECK:      Name: ENTRY
; CHECK-NEXT: Value: 0x14
; CHECK:      Hex dump of section '.text':
; CHECK-NEXT: 0x00000000 00000000 12341234 41004142 43440099
; CHECK-NEXT: 0x00000010 56785500 02010008 0014000c 0010
