; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

declare i32 @llvm.ctlz.i32(i32, i1)

; i32 ctlz should lower to libcall.

define i32 @ctlz32(i32 %a) {
; CHECK-LABEL: ctlz32:
; CHECK: CALL __clzsi2
entry:
  %r = call i32 @llvm.ctlz.i32(i32 %a, i1 false)
  ret i32 %r
}
