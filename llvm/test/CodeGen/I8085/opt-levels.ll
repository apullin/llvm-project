; RUN: llc -mattr=i8085,sram -O1 < %s -march=i8085 -verify-machineinstrs | FileCheck %s --check-prefix=O1
; RUN: llc -mattr=i8085,sram -O2 < %s -march=i8085 -verify-machineinstrs | FileCheck %s --check-prefix=O2
; RUN: llc -mattr=i8085,sram -O3 < %s -march=i8085 -verify-machineinstrs | FileCheck %s --check-prefix=O3

; Verify the optimization pipelines do not crash on mixed IR patterns.

define i32 @opt_math(i32 %a, i32 %b, i8 %shift) {
; O1-LABEL: opt_math:
; O1: RET
; O2-LABEL: opt_math:
; O2: RET
; O3-LABEL: opt_math:
; O3: RET
entry:
  %sa = zext i8 %shift to i32
  %sh = shl i32 %a, %sa
  %sum = add i32 %sh, %b
  %andv = and i32 %sum, 123456
  %orv = or i32 %andv, %a
  ret i32 %orv
}

define i16 @opt_mem(ptr %p, i16 %v) {
; O1-LABEL: opt_mem:
; O1: RET
; O2-LABEL: opt_mem:
; O2: RET
; O3-LABEL: opt_mem:
; O3: RET
entry:
  store volatile i16 %v, ptr %p
  %r = load volatile i16, ptr %p
  ret i16 %r
}

define i32 @opt_loop(i32 %n) {
; O1-LABEL: opt_loop:
; O1: RET
; O2-LABEL: opt_loop:
; O2: RET
; O3-LABEL: opt_loop:
; O3: RET
entry:
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %loop ]
  %acc = phi i32 [ 1, %entry ], [ %acc.next, %loop ]
  %acc.next = add i32 %acc, %i
  %i.next = add i32 %i, 1
  %cmp = icmp ult i32 %i.next, %n
  br i1 %cmp, label %loop, label %exit

exit:
  ret i32 %acc.next
}
