; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Multiple allocas with stores/loads to exercise stack frame setup.

define i16 @stack_alloca(i8 %x, i16 %y) {
; CHECK-LABEL: stack_alloca:
; CHECK: SPHL
; CHECK: RET
entry:
  %a = alloca i8, align 1
  %b = alloca i16, align 1
  %c = alloca [6 x i8], align 1
  store i8 %x, ptr %a, align 1
  store i16 %y, ptr %b, align 1
  %p = getelementptr inbounds [6 x i8], ptr %c, i16 0, i16 5
  store i8 42, ptr %p, align 1
  %la = load i8, ptr %a, align 1
  %lb = load i16, ptr %b, align 1
  %la16 = zext i8 %la to i16
  %sum = add i16 %lb, %la16
  ret i16 %sum
}
