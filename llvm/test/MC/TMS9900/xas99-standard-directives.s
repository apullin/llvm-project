; RUN: llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
; RUN:   -filetype=obj %s -o %t.o
; RUN: llvm-readobj --sections --symbols --hex-dump=.text %t.o \
; RUN:   | FileCheck %s

* Traditional full-line comment.
        IDT 'PROGRAM'
        AORG >0002
start   BYTE >11
aligned EVEN
        DATA >2233
block   BSS 2
endblk  BES 3
after   BYTE >44
        STRI 'HI'
ignored TITL 'ignored directive'
        RT
        END

; CHECK:      Name: .text
; CHECK:      Size: 18
; CHECK:      Name: START
; CHECK-NEXT: Value: 0x2
; CHECK:      Name: ALIGNED
; CHECK-NEXT: Value: 0x4
; CHECK:      Name: BLOCK
; CHECK-NEXT: Value: 0x6
; CHECK:      Name: ENDBLK
; CHECK-NEXT: Value: 0xB
; CHECK:      Name: AFTER
; CHECK-NEXT: Value: 0xB
; CHECK:      Name: IGNORED
; CHECK-NEXT: Value: 0xF
; CHECK:      Hex dump of section '.text':
; CHECK-NEXT: 0x00000000 00001100 22330000 00000044 02484900
; CHECK-NEXT: 0x00000010 045b
