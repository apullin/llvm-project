; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Force register pressure across a call to stress spills.

declare i16 @callee_many(i16, i16, i16, i16, i16, i16, i16, i16)

define i16 @spill_pressure(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e, i16 %f, i16 %g, i16 %h) {
; CHECK-LABEL: spill_pressure:
; CHECK: CALL callee_many
; CHECK: RET
entry:
  %call = call i16 @callee_many(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e, i16 %f, i16 %g, i16 %h)
  %s1 = add i16 %a, %b
  %s2 = add i16 %c, %d
  %s3 = add i16 %e, %f
  %s4 = add i16 %g, %h
  %s5 = add i16 %s1, %s2
  %s6 = add i16 %s3, %s4
  %s7 = add i16 %s5, %s6
  %s8 = add i16 %s7, %call
  ret i16 %s8
}
