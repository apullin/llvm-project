; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; i64 arguments should lower cleanly through the calling convention.

define i64 @callee64(i64 %a, i64 %b) {
entry:
  %sum = add i64 %a, %b
  ret i64 %sum
}

define i64 @caller64(i64 %x, i64 %y) {
; CHECK-LABEL: caller64:
; CHECK: CALL callee64
; CHECK: RET
entry:
  %r = call i64 @callee64(i64 %x, i64 %y)
  ret i64 %r
}
