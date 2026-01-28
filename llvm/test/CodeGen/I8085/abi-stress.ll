; RUN: llc -O0 -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; ABI stress: mixed args, varargs calls, and wide return values.

declare void @sink_mix(i8, i16, i32, i64, i8, i16, i32, i64)
declare i32 @sink_var(i8, ...)

define void @call_sink_mix(i8 %a, i16 %b, i32 %c, i64 %d, i8 %e, i16 %f, i32 %g, i64 %h) {
; CHECK-LABEL: call_sink_mix:
; CHECK: CALL sink_mix
; CHECK: RET
entry:
  call void @sink_mix(i8 %a, i16 %b, i32 %c, i64 %d, i8 %e, i16 %f, i32 %g, i64 %h)
  ret void
}

define i32 @call_sink_var(i8 %tag, i32 %a, i64 %b, i8 %c) {
; CHECK-LABEL: call_sink_var:
; CHECK: CALL sink_var
; CHECK: RET
entry:
  %r = call i32 (i8, ...) @sink_var(i8 %tag, i32 %a, i64 %b, i8 %c)
  ret i32 %r
}

define i64 @mix_args_return(i8 %a, i16 %b, i32 %c, i64 %d, i8 %e, i16 %f, i32 %g, i64 %h) {
; CHECK-LABEL: mix_args_return:
; CHECK: RET
entry:
  %x = zext i8 %a to i64
  %y = zext i16 %b to i64
  %z = zext i32 %c to i64
  %t = add i64 %x, %y
  %u = add i64 %t, %z
  %v = add i64 %u, %d
  %w = add i64 %v, %h
  ret i64 %w
}
