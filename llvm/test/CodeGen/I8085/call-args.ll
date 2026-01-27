; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Ensure stack argument placement uses distinct offsets before CALL.

define i8 @callee(i8 %a, i16 %b) {
entry:
  %b8 = trunc i16 %b to i8
  %sum = add i8 %a, %b8
  ret i8 %sum
}

define i8 @caller(i8 %x, i16 %y) {
; CHECK-LABEL: caller:
; CHECK-DAG: LXI H, 0
; CHECK-DAG: LXI H, 1
; CHECK-DAG: LXI H, 2
; CHECK: CALL callee
entry:
  %call = call i8 @callee(i8 %x, i16 %y)
  ret i8 %call
}
