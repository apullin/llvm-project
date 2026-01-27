; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

define i8 @mul8(i8 %a, i8 %b) {
; CHECK-LABEL: mul8:
; CHECK: CALL __mul8
entry:
  %r = mul i8 %a, %b
  ret i8 %r
}

define i8 @sdiv8(i8 %a, i8 %b) {
; CHECK-LABEL: sdiv8:
; CHECK: CALL __sdiv8
entry:
  %r = sdiv i8 %a, %b
  ret i8 %r
}

define i8 @udiv8(i8 %a, i8 %b) {
; CHECK-LABEL: udiv8:
; CHECK: CALL __udiv8
entry:
  %r = udiv i8 %a, %b
  ret i8 %r
}

define i8 @srem8(i8 %a, i8 %b) {
; CHECK-LABEL: srem8:
; CHECK: CALL __srem8
entry:
  %r = srem i8 %a, %b
  ret i8 %r
}

define i8 @urem8(i8 %a, i8 %b) {
; CHECK-LABEL: urem8:
; CHECK: CALL __urem8
entry:
  %r = urem i8 %a, %b
  ret i8 %r
}
