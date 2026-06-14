; RUN: llc -O2 -mtriple=i8085-unknown-elf < %s | FileCheck %s

target triple = "i8085-unknown-unknown"

define i16 @sum3_i16(i16 %a, i16 %b, i16 %c) nounwind {
entry:
  %ab = add i16 %a, %b
  %abc = add i16 %ab, %c
  ret i16 %abc
}

define i8 @mix_i8(i8 %a, i8 %b, i8 %c, i8 %d) nounwind {
entry:
  %ab = add i8 %a, %b
  %cd = add i8 %c, %d
  %r = xor i8 %ab, %cd
  ret i8 %r
}

; CHECK-LABEL: sum3_i16:
; CHECK:       LXI H, 2
; CHECK-NEXT:  DAD SP
; CHECK:       INX H
; CHECK:       INX H
; CHECK-NOT:   DAD SP
; CHECK:       RET

; CHECK-LABEL: mix_i8:
; CHECK:       LXI H, 5
; CHECK-NEXT:  DAD SP
; CHECK:       DCX H
; CHECK:       DCX H
; CHECK-NOT:   DAD SP
; CHECK:       RET
