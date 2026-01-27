; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

define i64 @mul64(i64 %a, i64 %b) {
; CHECK-LABEL: mul64:
; CHECK: CALL __muldi3
entry:
  %r = mul i64 %a, %b
  ret i64 %r
}

define i64 @sdiv64(i64 %a, i64 %b) {
; CHECK-LABEL: sdiv64:
; CHECK: CALL __divdi3
entry:
  %r = sdiv i64 %a, %b
  ret i64 %r
}

define i64 @udiv64(i64 %a, i64 %b) {
; CHECK-LABEL: udiv64:
; CHECK: CALL __udivdi3
entry:
  %r = udiv i64 %a, %b
  ret i64 %r
}

define i64 @srem64(i64 %a, i64 %b) {
; CHECK-LABEL: srem64:
; CHECK: CALL __moddi3
entry:
  %r = srem i64 %a, %b
  ret i64 %r
}

define i64 @urem64(i64 %a, i64 %b) {
; CHECK-LABEL: urem64:
; CHECK: CALL __umoddi3
entry:
  %r = urem i64 %a, %b
  ret i64 %r
}

define i64 @shl64(i64 %a, i8 %b) {
; CHECK-LABEL: shl64:
; CHECK: CALL __ashldi3
entry:
  %bb = zext i8 %b to i64
  %r = shl i64 %a, %bb
  ret i64 %r
}

define i64 @lshr64(i64 %a, i8 %b) {
; CHECK-LABEL: lshr64:
; CHECK: CALL __lshrdi3
entry:
  %bb = zext i8 %b to i64
  %r = lshr i64 %a, %bb
  ret i64 %r
}

define i64 @ashr64(i64 %a, i8 %b) {
; CHECK-LABEL: ashr64:
; CHECK: CALL __ashrdi3
entry:
  %bb = zext i8 %b to i64
  %r = ashr i64 %a, %bb
  ret i64 %r
}
