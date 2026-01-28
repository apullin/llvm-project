; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

declare i8 @llvm.fshl.i8(i8, i8, i8)
declare i8 @llvm.fshr.i8(i8, i8, i8)

define i8 @fshl8(i8 %a, i8 %b, i8 %c) {
; CHECK-LABEL: fshl8:
; CHECK: RET
entry:
  %r = call i8 @llvm.fshl.i8(i8 %a, i8 %b, i8 %c)
  ret i8 %r
}

define i8 @fshr8(i8 %a, i8 %b, i8 %c) {
; CHECK-LABEL: fshr8:
; CHECK: RET
entry:
  %r = call i8 @llvm.fshr.i8(i8 %a, i8 %b, i8 %c)
  ret i8 %r
}
