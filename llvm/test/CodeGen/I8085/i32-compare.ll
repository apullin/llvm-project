; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; i32 comparisons should lower without leaving compare/select pseudos.

define i8 @cmp_eq_i32(i32 %a, i32 %b) {
; CHECK-LABEL: cmp_eq_i32:
; CHECK-NOT: SET_
; CHECK-NOT: JMP_32_IF
; CHECK: RET
entry:
  %cmp = icmp eq i32 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

define i8 @cmp_ult_i32(i32 %a, i32 %b) {
; CHECK-LABEL: cmp_ult_i32:
; CHECK-NOT: SET_
; CHECK-NOT: JMP_32_IF
; CHECK: RET
entry:
  %cmp = icmp ult i32 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

define i8 @cmp_slt_i32(i32 %a, i32 %b) {
; CHECK-LABEL: cmp_slt_i32:
; CHECK-NOT: SET_
; CHECK-NOT: JMP_32_IF
; CHECK: RET
entry:
  %cmp = icmp slt i32 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

define i8 @cmp_sgt_i32(i32 %a, i32 %b) {
; CHECK-LABEL: cmp_sgt_i32:
; CHECK-NOT: SET_
; CHECK-NOT: JMP_32_IF
; CHECK: RET
entry:
  %cmp = icmp sgt i32 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}
