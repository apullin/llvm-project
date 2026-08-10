; RUN: llc -mtriple=tms9900 -O2 < %s | FileCheck %s
;
; Test variable argument function handling.
; va_start should set up the argument pointer to read variadic args
; from the stack. va_arg loads the next argument and advances the pointer.

target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-i64:16:16-f32:16:16-f64:16:16-n16-S32"
target triple = "tms9900"

declare void @llvm.va_start(ptr)
declare void @llvm.va_end(ptr)

; --- Simple varargs function: read two variadic args ---
; The function allocates stack space for the va_list, calls va_start,
; reads two i16 arguments, then returns their sum. The named argument is also
; stack-passed, so after the 2-byte local the first variadic value is at +4.
; CHECK-LABEL: va_func:
; CHECK: DECT{{[ \t]+}}R10
; CHECK: MOV{{[ \t]+}}R10,[[AP:R[0-9]+]]
; CHECK-NEXT: AI{{[ \t]+}}[[AP]],4
; CHECK: MOV{{[ \t]+}}*[[AP]],R0
; CHECK: MOV{{[ \t]+}}*[[AP]],R1
; CHECK: A{{[ \t]+}}R1,R0
; CHECK: B{{[ \t]+}}*R11

define i16 @va_func(i16 %n, ...) {
  %ap = alloca ptr
  call void @llvm.va_start(ptr %ap)
  %first = va_arg ptr %ap, i16
  %second = va_arg ptr %ap, i16
  call void @llvm.va_end(ptr %ap)
  %sum = add i16 %first, %second
  ret i16 %sum
}

; A variadic caller places the named argument and both variadic arguments in
; consecutive stack slots. No argument value is passed in R0-R3.
; CHECK-LABEL: call_va:
; CHECK:       DECT R10
; CHECK-NEXT:  MOV R11,*R10
; CHECK-NEXT:  AI R10,-10
; CHECK-NEXT:  LI R0,23
; CHECK-NEXT:  MOV R0,@4(R10)
; CHECK-NEXT:  LI R0,42
; CHECK-NEXT:  MOV R0,@2(R10)
; CHECK-NEXT:  LI R0,2
; CHECK-NEXT:  MOV R0,*R10
; CHECK-NEXT:  BL @va_func
; CHECK-NEXT:  AI R10,10
; CHECK-NEXT:  MOV *R10+,R11
; CHECK-NEXT:  B *R11

define i16 @call_va() {
  %value = call i16 (i16, ...) @va_func(i16 2, i16 42, i16 23)
  ret i16 %value
}
