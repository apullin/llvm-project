; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

declare i16 @llvm.smin.i16(i16, i16)
declare i16 @llvm.smax.i16(i16, i16)
declare i16 @llvm.umin.i16(i16, i16)
declare i16 @llvm.umax.i16(i16, i16)
declare i16 @llvm.abs.i16(i16, i1)

define i16 @smin16(i16 %a, i16 %b) {
; CHECK-LABEL: smin16:
; CHECK: RET
entry:
  %r = call i16 @llvm.smin.i16(i16 %a, i16 %b)
  ret i16 %r
}

define i16 @smax16(i16 %a, i16 %b) {
; CHECK-LABEL: smax16:
; CHECK: RET
entry:
  %r = call i16 @llvm.smax.i16(i16 %a, i16 %b)
  ret i16 %r
}

define i16 @umin16(i16 %a, i16 %b) {
; CHECK-LABEL: umin16:
; CHECK: RET
entry:
  %r = call i16 @llvm.umin.i16(i16 %a, i16 %b)
  ret i16 %r
}

define i16 @umax16(i16 %a, i16 %b) {
; CHECK-LABEL: umax16:
; CHECK: RET
entry:
  %r = call i16 @llvm.umax.i16(i16 %a, i16 %b)
  ret i16 %r
}

define i16 @abs16(i16 %a) {
; CHECK-LABEL: abs16:
; CHECK: RET
entry:
  %r = call i16 @llvm.abs.i16(i16 %a, i1 false)
  ret i16 %r
}
