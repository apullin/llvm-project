; RUN: llc -O0 -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Constant divisors/remainders should still lower to libcalls.

define i8 @sdiv8_const(i8 %a) {
; CHECK-LABEL: sdiv8_const:
; CHECK: CALL __sdiv8
entry:
  %r = sdiv i8 %a, 3
  ret i8 %r
}

define i8 @srem8_const_neg(i8 %a) {
; CHECK-LABEL: srem8_const_neg:
; CHECK: CALL __srem8
entry:
  %r = srem i8 %a, -5
  ret i8 %r
}

define i16 @udiv16_const(i16 %a) {
; CHECK-LABEL: udiv16_const:
; CHECK: CALL __udiv16
entry:
  %r = udiv i16 %a, 10
  ret i16 %r
}

define i16 @urem16_const(i16 %a) {
; CHECK-LABEL: urem16_const:
; CHECK: CALL __urem16
entry:
  %r = urem i16 %a, 7
  ret i16 %r
}

define i32 @sdiv32_const(i32 %a) {
; CHECK-LABEL: sdiv32_const:
; CHECK: CALL __sdiv32
entry:
  %r = sdiv i32 %a, -9
  ret i32 %r
}

define i32 @srem32_const(i32 %a) {
; CHECK-LABEL: srem32_const:
; CHECK: CALL __srem32
entry:
  %r = srem i32 %a, 11
  ret i32 %r
}

define i64 @udiv64_const(i64 %a) {
; CHECK-LABEL: udiv64_const:
; CHECK: CALL __udivdi3
entry:
  %r = udiv i64 %a, 13
  ret i64 %r
}

define i64 @urem64_const(i64 %a) {
; CHECK-LABEL: urem64_const:
; CHECK: CALL __umoddi3
entry:
  %r = urem i64 %a, 17
  ret i64 %r
}
