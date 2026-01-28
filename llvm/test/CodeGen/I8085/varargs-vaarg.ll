; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Exercise va_start/va_arg lowering for stack-based varargs.

declare void @llvm.va_start(i8*)
declare void @llvm.va_end(i8*)

define i16 @va_get(i16 %count, ...) {
; CHECK-LABEL: va_get:
; CHECK: RET
entry:
  %ap = alloca i8*
  %ap8 = bitcast i8** %ap to i8*
  call void @llvm.va_start(i8* %ap8)
  %v0 = va_arg i8* %ap8, i16
  %v1 = va_arg i8* %ap8, i8
  %v1z = zext i8 %v1 to i16
  %sum = add i16 %v0, %v1z
  call void @llvm.va_end(i8* %ap8)
  ret i16 %sum
}
