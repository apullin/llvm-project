; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Mixed-size arguments and return to stress argument loads.

define i16 @mixed_args(i8 %a, i16 %b, i32 %c, i8 %d, i16 %e) {
; CHECK-LABEL: mixed_args:
; CHECK: RET
entry:
  %a16 = zext i8 %a to i16
  %d16 = zext i8 %d to i16
  %c16 = trunc i32 %c to i16
  %sum1 = add i16 %b, %e
  %sum2 = add i16 %a16, %d16
  %sum3 = add i16 %sum1, %sum2
  %sum4 = add i16 %sum3, %c16
  ret i16 %sum4
}
