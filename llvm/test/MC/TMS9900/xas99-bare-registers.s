; RUN: llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
; RUN:   -filetype=obj %s -o %t.o
; RUN: llvm-readobj --hex-dump=.text %t.o | FileCheck %s

        MOVB 1,@>1234
        LI 0,-1
        MOV @4(2),*3+
        SLA 4,0
        STST 5
        CLR >F
        B *11

; CHECK:      Hex dump of section '.text':
; CHECK-NEXT: 0x00000000 d8011234 0200ffff cce20004 0a0402c5
; CHECK-NEXT: 0x00000010 04cf045b
