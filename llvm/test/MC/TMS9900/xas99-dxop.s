; RUN: llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
; RUN:   -filetype=obj %s -o %t.o
; RUN: llvm-readobj --relocations --symbols --hex-dump=.text %t.o \
; RUN:   | FileCheck %s

        BYTE 0
        EVEN
site    DXOP service,2
call    SERVICE @site

; CHECK:      0x4 R_TMS9900_16 .text 0x2
; CHECK:      Name: SITE
; CHECK-NEXT: Value: 0x2
; CHECK:      Name: CALL
; CHECK-NEXT: Value: 0x2
; CHECK:      Hex dump of section '.text':
; CHECK-NEXT: 0x00000000 00002ca0 0000
