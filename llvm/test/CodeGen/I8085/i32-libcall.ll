; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

define i32 @mul32(i32 %a, i32 %b) {
; CHECK-LABEL: mul32:
; CHECK: CALL __mul32
entry:
  %r = mul i32 %a, %b
  ret i32 %r
}

define i32 @sdiv32(i32 %a, i32 %b) {
; CHECK-LABEL: sdiv32:
; CHECK: CALL __sdiv32
entry:
  %r = sdiv i32 %a, %b
  ret i32 %r
}

define i32 @udiv32(i32 %a, i32 %b) {
; CHECK-LABEL: udiv32:
; CHECK: CALL __udiv32
entry:
  %r = udiv i32 %a, %b
  ret i32 %r
}

define i32 @srem32(i32 %a, i32 %b) {
; CHECK-LABEL: srem32:
; CHECK: CALL __srem32
entry:
  %r = srem i32 %a, %b
  ret i32 %r
}

define i32 @urem32(i32 %a, i32 %b) {
; CHECK-LABEL: urem32:
; CHECK: CALL __urem32
entry:
  %r = urem i32 %a, %b
  ret i32 %r
}

declare i32 @llvm.ctlz.i32(i32, i1)

define i32 @clz32(i32 %a) {
; CHECK-LABEL: clz32:
; CHECK: CALL __clzsi2
entry:
  %r = call i32 @llvm.ctlz.i32(i32 %a, i1 false)
  ret i32 %r
}
