; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Varargs call: ensure caller lays out extra args on stack and emits CALL.

declare void @vfoo(i16, ...)

define void @caller_varargs(i16 %a, i8 %b, i32 %c) {
; CHECK-LABEL: caller_varargs:
; CHECK: CALL vfoo
; CHECK: RET
entry:
  call void (i16, ...) @vfoo(i16 %a, i8 %b, i32 %c, i16 4660, i8 7)
  ret void
}
