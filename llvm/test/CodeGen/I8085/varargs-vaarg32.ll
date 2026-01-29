; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

declare void @llvm.va_start(i8*)
declare void @llvm.va_end(i8*)

define i32 @va_get32(i16 %count, ...) {
; CHECK-LABEL: va_get32:
; CHECK: RET
entry:
  %ap = alloca i8*
  %ap8 = bitcast i8** %ap to i8*
  call void @llvm.va_start(i8* %ap8)
  %v0 = va_arg i8* %ap8, i32
  %v1 = va_arg i8* %ap8, i64
  %v1lo = trunc i64 %v1 to i32
  %sum = add i32 %v0, %v1lo
  call void @llvm.va_end(i8* %ap8)
  ret i32 %sum
}
