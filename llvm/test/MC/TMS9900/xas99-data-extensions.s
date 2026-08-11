; RUN: llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
; RUN:   -I %S/Inputs -filetype=obj %s -o %t.o
; RUN: llvm-readobj --symbols --hex-dump=.text %t.o | FileCheck %s

binval  EQU :1010
start   DATA :01011010,:11110000
        BYTE binval
; The exact power 100^3 must use biased exponent >43, without host FP error.
float   FLOA 1,0.1,-1.1,0,1.2345678901234567890,1000000
text    TEXT >1234567890abcdef,>123,->123456
binary  BCOPY "xas99-bcopy.bin"
after   BYTE >55

; CHECK:      Name: START
; CHECK-NEXT: Value: 0x0
; CHECK:      Name: FLOAT
; CHECK-NEXT: Value: 0x5
; CHECK:      Name: TEXT
; CHECK-NEXT: Value: 0x35
; CHECK:      Name: BINARY
; CHECK-NEXT: Value: 0x42
; CHECK:      Name: AFTER
; CHECK-NEXT: Value: 0x46
; CHECK:      Hex dump of section '.text':
; CHECK-NEXT: 0x00000000 005a00f0 0a400100 00000000 003f0a00
; CHECK-NEXT: 0x00000010 00000000 00bfff0a 00000000 00000000
; CHECK-NEXT: 0x00000020 00000000 00400117 2d435901 17430100
; CHECK-NEXT: 0x00000030 00000000 00123456 7890abcd ef123012
; CHECK-NEXT: 0x00000040 34aa5859 5a0a55
