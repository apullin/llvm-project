; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Ensure i32 stack arguments lower without SelectionDAG failures.

define i8 @callee(i8 %a, i16 %b, i32 %c) {
entry:
  %b8 = trunc i16 %b to i8
  %c8 = trunc i32 %c to i8
  %sum = add i8 %a, %b8
  %sum2 = add i8 %sum, %c8
  ret i8 %sum2
}

define i8 @caller(i8 %x, i16 %y, i32 %z) {
; CHECK-LABEL: caller:
; CHECK: CALL callee
entry:
  %call = call i8 @callee(i8 %x, i16 %y, i32 %z)
  ret i8 %call
}
