; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Ensure large stack offsets don't assert and are materialized via LXI/DAD.

define void @stack_large() {
; CHECK-LABEL: stack_large:
; CHECK: LXI H,
; CHECK: DAD SP
; CHECK: MOV M,
  %arr = alloca [80 x i8], align 1
  %p = getelementptr inbounds [80 x i8], ptr %arr, i16 0, i16 79
  store i8 1, ptr %p, align 1
  ret void
}
