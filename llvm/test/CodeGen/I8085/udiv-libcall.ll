; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

define i16 @udiv16(i16 %a, i16 %b) {
; CHECK-LABEL: udiv16:
; CHECK: CALL __udiv16
entry:
  %r = udiv i16 %a, %b
  ret i16 %r
}

define i16 @urem16(i16 %a, i16 %b) {
; CHECK-LABEL: urem16:
; CHECK: CALL __urem16
entry:
  %r = urem i16 %a, %b
  ret i16 %r
}
