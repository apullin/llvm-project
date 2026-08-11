; RUN: llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
; RUN:   -I %S/Inputs -filetype=obj %s -o %t.o
; RUN: llvm-readobj --symbols --hex-dump=.text %t.o | FileCheck %s

        BYTE >56
before  COPY "xas99-copy.inc"
after   BYTE >78

; CHECK:      Name: BEFORE
; CHECK-NEXT: Value: 0x1
; CHECK:      Name: COPIED
; CHECK-NEXT: Value: 0x1
; CHECK:      Name: AFTER
; CHECK-NEXT: Value: 0x3
; CHECK:      Hex dump of section '.text':
; CHECK-NEXT: 0x00000000 56123478
