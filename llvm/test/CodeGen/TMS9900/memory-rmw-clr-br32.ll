; RUN: llc -mtriple=tms9900 -O2 -verify-machineinstrs < %s | FileCheck %s

@g_word = global i16 0, align 2
declare void @callee()

define void @inc_global() {
; CHECK-LABEL: inc_global:
; CHECK-NEXT:  ; %bb.0:
; CHECK-NEXT:  INC @g_word
; CHECK-NEXT:  B *R11
  %v = load i16, ptr @g_word, align 2
  %inc = add i16 %v, 1
  store i16 %inc, ptr @g_word, align 2
  ret void
}

define void @clr_and_seto_mem(ptr %p) {
; CHECK-LABEL: clr_and_seto_mem:
; CHECK-DAG:   CLR *R0
; CHECK-DAG:   SETO @g_word
; CHECK:       B *R11
  store i16 0, ptr %p, align 2
  store i16 -1, ptr @g_word, align 2
  ret void
}

define void @br_slt32(i32 %a, i32 %b) {
; CHECK-LABEL: br_slt32:
; CHECK-NOT:   SETO
; CHECK-NOT:   SZC
; CHECK:       C R0,R2
; CHECK:       C R1,R3
  %cmp = icmp slt i32 %a, %b
  br i1 %cmp, label %then, label %done
then:
  tail call void @callee()
  ret void
done:
  ret void
}

define void @movb_zero_branch(ptr %p) {
; CHECK-LABEL: movb_zero_branch:
; CHECK:       MOVB *R0,R0
; CHECK-NOT:   SRL R0,8
; CHECK-NEXT:  JEQ
  %b = load i8, ptr %p, align 1
  %nz = icmp ne i8 %b, 0
  br i1 %nz, label %then, label %done
then:
  tail call void @callee()
  ret void
done:
  ret void
}
