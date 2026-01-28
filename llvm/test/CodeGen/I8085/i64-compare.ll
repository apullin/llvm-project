; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; i64 comparisons should lower without leaving compare/select pseudos.

define i8 @cmp_eq_i64_to_i8(i64 %a, i64 %b) {
; CHECK-LABEL: cmp_eq_i64_to_i8:
; CHECK-NOT: SET_
; CHECK-NOT: JMP_32_IF
; CHECK: RET
entry:
  %cmp = icmp eq i64 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

define i8 @cmp_ne_i64_to_i8(i64 %a, i64 %b) {
; CHECK-LABEL: cmp_ne_i64_to_i8:
; CHECK-NOT: SET_
; CHECK-NOT: JMP_32_IF
; CHECK: RET
entry:
  %cmp = icmp ne i64 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

define i8 @cmp_ult_i64_to_i8(i64 %a, i64 %b) {
; CHECK-LABEL: cmp_ult_i64_to_i8:
; CHECK-NOT: SET_
; CHECK-NOT: JMP_32_IF
; CHECK: RET
entry:
  %cmp = icmp ult i64 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

define i8 @cmp_slt_i64_to_i8(i64 %a, i64 %b) {
; CHECK-LABEL: cmp_slt_i64_to_i8:
; CHECK-NOT: SET_
; CHECK-NOT: JMP_32_IF
; CHECK: RET
entry:
  %cmp = icmp slt i64 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

define i8 @cmp_sgt_i64_to_i8(i64 %a, i64 %b) {
; CHECK-LABEL: cmp_sgt_i64_to_i8:
; CHECK-NOT: SET_
; CHECK-NOT: JMP_32_IF
; CHECK: RET
entry:
  %cmp = icmp sgt i64 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

define i8 @br_eq_i64(i64 %a, i64 %b) {
; CHECK-LABEL: br_eq_i64:
; CHECK: RET
entry:
  %cmp = icmp eq i64 %a, %b
  br i1 %cmp, label %t, label %f

t:
  ret i8 1

f:
  ret i8 0
}

define i8 @br_slt_i64(i64 %a, i64 %b) {
; CHECK-LABEL: br_slt_i64:
; CHECK: RET
entry:
  %cmp = icmp slt i64 %a, %b
  br i1 %cmp, label %t, label %f

t:
  ret i8 1

f:
  ret i8 0
}
