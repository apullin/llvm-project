; RUN: llc -mtriple=tms9900 -O0 < %s | FileCheck %s
;
; R10 is 4-byte aligned at function entry and must remain 4-byte aligned at
; every call site. Saving the 2-byte link register therefore requires a
; 2-byte pad even when a non-leaf function has no other frame objects.

; CHECK-LABEL: aligned_callee:
; CHECK:       AI R10,-4
; CHECK:       MOV R10,R0
; CHECK:       ANDI R0,3
; CHECK:       AI R10,4
; CHECK:       B *R11

define i16 @aligned_callee() noinline {
entry:
  %slot = alloca i32, align 4
  %address = ptrtoint ptr %slot to i16
  %misalignment = and i16 %address, 3
  ret i16 %misalignment
}

; CHECK-LABEL: alignment_caller:
; CHECK:       DECT R10
; CHECK-NEXT:  MOV R11,*R10
; CHECK-NEXT:  DECT R10
; CHECK-NEXT:  BL @aligned_callee
; CHECK-NEXT:  INCT R10
; CHECK-NEXT:  MOV *R10+,R11
; CHECK-NEXT:  B *R11

define i16 @alignment_caller() {
entry:
  %result = call i16 @aligned_callee()
  ret i16 %result
}
