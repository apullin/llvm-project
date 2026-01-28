; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

declare i8 @llvm.cttz.i8(i8, i1)
declare i16 @llvm.cttz.i16(i16, i1)
declare i8 @llvm.ctpop.i8(i8)
declare i16 @llvm.ctpop.i16(i16)

define i8 @cttz8(i8 %a) {
; CHECK-LABEL: cttz8:
; CHECK: RET
entry:
  %r = call i8 @llvm.cttz.i8(i8 %a, i1 false)
  ret i8 %r
}

define i16 @cttz16(i16 %a) {
; CHECK-LABEL: cttz16:
; CHECK: RET
entry:
  %r = call i16 @llvm.cttz.i16(i16 %a, i1 false)
  ret i16 %r
}

define i8 @ctpop8(i8 %a) {
; CHECK-LABEL: ctpop8:
; CHECK: RET
entry:
  %r = call i8 @llvm.ctpop.i8(i8 %a)
  ret i8 %r
}

define i16 @ctpop16(i16 %a) {
; CHECK-LABEL: ctpop16:
; CHECK: RET
entry:
  %r = call i16 @llvm.ctpop.i16(i16 %a)
  ret i16 %r
}
