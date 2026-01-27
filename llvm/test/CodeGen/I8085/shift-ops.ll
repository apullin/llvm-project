; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

define i8 @shifts8(i8 %a, i8 %b) {
; CHECK-LABEL: shifts8:
; CHECK-NOT: SHL_8
; CHECK-NOT: SRA_8
; CHECK-NOT: SRL_8
; CHECK: RET
entry:
  %shl = shl i8 %a, %b
  %shr = lshr i8 %shl, %b
  %sar = ashr i8 %shr, %b
  ret i8 %sar
}

define i16 @shifts16(i16 %a, i8 %b) {
; CHECK-LABEL: shifts16:
; CHECK-NOT: SHL_16
; CHECK-NOT: SRA_16
; CHECK-NOT: SRL_16
; CHECK: RET
entry:
  %b16 = zext i8 %b to i16
  %shl = shl i16 %a, %b16
  %shr = lshr i16 %shl, %b16
  %sar = ashr i16 %shr, %b16
  ret i16 %sar
}
