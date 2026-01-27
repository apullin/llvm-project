; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

declare i32 @llvm.cttz.i32(i32, i1)
declare i64 @llvm.cttz.i64(i64, i1)
declare i32 @llvm.ctpop.i32(i32)
declare i64 @llvm.ctpop.i64(i64)

define i32 @cttz32(i32 %a) {
; CHECK-LABEL: cttz32:
; CHECK: CALL __ctzsi2
entry:
  %r = call i32 @llvm.cttz.i32(i32 %a, i1 false)
  ret i32 %r
}

define i64 @cttz64(i64 %a) {
; CHECK-LABEL: cttz64:
; CHECK: CALL __ctzdi2
entry:
  %r = call i64 @llvm.cttz.i64(i64 %a, i1 false)
  ret i64 %r
}

define i32 @ctpop32(i32 %a) {
; CHECK-LABEL: ctpop32:
entry:
  %r = call i32 @llvm.ctpop.i32(i32 %a)
  ret i32 %r
}

define i64 @ctpop64(i64 %a) {
; CHECK-LABEL: ctpop64:
entry:
  %r = call i64 @llvm.ctpop.i64(i64 %a)
  ret i64 %r
}
