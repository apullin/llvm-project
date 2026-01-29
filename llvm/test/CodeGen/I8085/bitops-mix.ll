; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

define i8 @bitmix_i8(i8 %a, i8 %b) {
; CHECK-LABEL: bitmix_i8:
; CHECK: RET
entry:
  %c = and i8 %a, 240
  %d = or i8 %c, %b
  %e = xor i8 %d, 170
  ret i8 %e
}

define i16 @bitmix_i16(i16 %a, i16 %b) {
; CHECK-LABEL: bitmix_i16:
; CHECK: RET
entry:
  %c = and i16 %a, 61680
  %d = or i16 %c, %b
  %e = xor i16 %d, 43690
  ret i16 %e
}

define i8 @bitnot_i8(i8 %a) {
; CHECK-LABEL: bitnot_i8:
; CHECK: RET
entry:
  %n = xor i8 %a, -1
  ret i8 %n
}
