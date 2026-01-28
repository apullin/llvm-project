; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

@g = global [8 x i8] zeroinitializer, align 1
@h = global i16 0, align 1

; Global base + constant offset addressing.

define void @global_offset(i8 %x, i16 %y) {
; CHECK-LABEL: global_offset:
; CHECK: RET
entry:
  %p = getelementptr inbounds [8 x i8], ptr @g, i16 0, i16 5
  store i8 %x, ptr %p, align 1
  store i16 %y, ptr @h, align 1
  %q = getelementptr inbounds [8 x i8], ptr @g, i16 0, i16 2
  %v = load i8, ptr %q, align 1
  %v16 = zext i8 %v to i16
  %hv = load i16, ptr @h, align 1
  %sum = add i16 %hv, %v16
  store i16 %sum, ptr @h, align 1
  ret void
}
