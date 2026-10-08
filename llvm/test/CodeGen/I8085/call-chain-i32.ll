; RUN: llc -mattr=i8085,sram < %s -march=i8085 | FileCheck %s

; Multiple return values and call chains with i32 return (BC:DE).
; Tests calling convention round-trip for 32-bit values.

declare i32 @get_i32()
declare void @use_i32(i32)

; Return i32 from addition of two calls
define i32 @call_chain_i32() {
; CHECK-LABEL: call_chain_i32:
; CHECK: CALL get_i32
; CHECK: CALL get_i32
; CHECK: ADD M
; CHECK: ADC M
; CHECK: RET
entry:
  %a = call i32 @get_i32()
  %b = call i32 @get_i32()
  %sum = add i32 %a, %b
  ret i32 %sum
}

; Pass i32 result directly to another function
define void @call_passthrough() {
; CHECK-LABEL: call_passthrough:
; CHECK: CALL get_i32
; CHECK: CALL use_i32
; CHECK: RET
entry:
  %v = call i32 @get_i32()
  call void @use_i32(i32 %v)
  ret void
}

; Multiple i16 args on stack
declare i16 @add4(i16, i16, i16, i16)

define i16 @call_many_args(i16 %a, i16 %b, i16 %c, i16 %d) {
; CHECK-LABEL: call_many_args:
; CHECK: CALL add4
; CHECK: RET
entry:
  %r = call i16 @add4(i16 %a, i16 %b, i16 %c, i16 %d)
  ret i16 %r
}
