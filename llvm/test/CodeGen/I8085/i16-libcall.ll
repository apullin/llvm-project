; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

define i16 @mul16(i16 %a, i16 %b) {
; CHECK-LABEL: mul16:
; CHECK: CALL __mul16
entry:
  %r = mul i16 %a, %b
  ret i16 %r
}

define i16 @sdiv16(i16 %a, i16 %b) {
; CHECK-LABEL: sdiv16:
; CHECK: CALL __sdiv16
entry:
  %r = sdiv i16 %a, %b
  ret i16 %r
}

define i16 @udiv16(i16 %a, i16 %b) {
; CHECK-LABEL: udiv16:
; CHECK: CALL __udiv16
entry:
  %r = udiv i16 %a, %b
  ret i16 %r
}

define i16 @srem16(i16 %a, i16 %b) {
; CHECK-LABEL: srem16:
; CHECK: CALL __srem16
entry:
  %r = srem i16 %a, %b
  ret i16 %r
}

define i16 @urem16(i16 %a, i16 %b) {
; CHECK-LABEL: urem16:
; CHECK: CALL __urem16
entry:
  %r = urem i16 %a, %b
  ret i16 %r
}
