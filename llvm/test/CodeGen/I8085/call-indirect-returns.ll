; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Indirect call should return wider values without crashing.

define i32 @call_indirect_i32(ptr %fn, i16 %a, i16 %b) {
; CHECK-LABEL: call_indirect_i32:
; CHECK: PCHL
; CHECK: RET
entry:
  %r = call i32 %fn(i16 %a, i16 %b)
  ret i32 %r
}

define i64 @call_indirect_i64(ptr %fn) {
; CHECK-LABEL: call_indirect_i64:
; CHECK: PCHL
; CHECK: RET
entry:
  %r = call i64 %fn()
  ret i64 %r
}
