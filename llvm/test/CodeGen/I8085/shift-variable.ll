; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Variable shift amounts.

define i8 @var_shifts_i8(i8 %x, i8 %s) {
; CHECK-LABEL: var_shifts_i8:
; CHECK: RET
entry:
  %s1 = and i8 %s, 7
  %shl = shl i8 %x, %s1
  %shr = lshr i8 %shl, %s1
  %sar = ashr i8 %shr, %s1
  ret i8 %sar
}

define i16 @var_shifts_i16(i16 %x, i8 %s) {
; CHECK-LABEL: var_shifts_i16:
; CHECK: RET
entry:
  %s1 = and i8 %s, 15
  %s1_16 = zext i8 %s1 to i16
  %shl = shl i16 %x, %s1_16
  %shr = lshr i16 %shl, %s1_16
  %sar = ashr i16 %shr, %s1_16
  ret i16 %sar
}
