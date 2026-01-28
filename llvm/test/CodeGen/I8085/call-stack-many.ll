; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Stress stack argument passing with a mix of widths.

define i16 @callee(i8 %a, i16 %b, i32 %c, i8 %d, i16 %e, i32 %f, i8 %g, i16 %h) {
entry:
  %a16 = zext i8 %a to i16
  %d16 = zext i8 %d to i16
  %g16 = zext i8 %g to i16
  %c16 = trunc i32 %c to i16
  %f16 = trunc i32 %f to i16
  %sum0 = add i16 %a16, %b
  %sum1 = add i16 %sum0, %c16
  %sum2 = add i16 %sum1, %d16
  %sum3 = add i16 %sum2, %e
  %sum4 = add i16 %sum3, %f16
  %sum5 = add i16 %sum4, %g16
  %sum6 = add i16 %sum5, %h
  ret i16 %sum6
}

define i16 @caller(i8 %x, i16 %y, i32 %z, i8 %w, i16 %u, i32 %v, i8 %t, i16 %s) {
; CHECK-LABEL: caller:
; CHECK: CALL callee
; CHECK: RET
entry:
  %r = call i16 @callee(i8 %x, i16 %y, i32 %z, i8 %w, i16 %u, i32 %v, i8 %t, i16 %s)
  ret i16 %r
}
