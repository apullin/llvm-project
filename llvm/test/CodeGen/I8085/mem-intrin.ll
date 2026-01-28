; RUN: llc -O0 -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Mem intrinsics should lower to libcalls or simple sequences without crashes.

declare void @llvm.memcpy.p0i8.p0i8.i16(i8* nocapture, i8* nocapture, i16, i1 immarg)
declare void @llvm.memmove.p0i8.p0i8.i16(i8* nocapture, i8* nocapture, i16, i1 immarg)
declare void @llvm.memset.p0i8.i16(i8* nocapture, i8, i16, i1 immarg)

define void @do_memcpy(i8* %dst, i8* %src, i16 %n) {
; CHECK-LABEL: do_memcpy:
; CHECK: CALL memcpy
; CHECK: RET
entry:
  call void @llvm.memcpy.p0i8.p0i8.i16(i8* %dst, i8* %src, i16 %n, i1 false)
  ret void
}

define void @do_memmove(i8* %dst, i8* %src, i16 %n) {
; CHECK-LABEL: do_memmove:
; CHECK: CALL memmove
; CHECK: RET
entry:
  call void @llvm.memmove.p0i8.p0i8.i16(i8* %dst, i8* %src, i16 %n, i1 false)
  ret void
}

define void @do_memset(i8* %dst, i8 %val, i16 %n) {
; CHECK-LABEL: do_memset:
; CHECK: CALL memset
; CHECK: RET
entry:
  call void @llvm.memset.p0i8.i16(i8* %dst, i8 %val, i16 %n, i1 false)
  ret void
}
