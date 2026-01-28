; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Returning i1 should lower without leaving compare pseudos.

define i1 @ret_i1_eq(i8 %a, i8 %b) {
; CHECK-LABEL: ret_i1_eq:
; CHECK-NOT: SET_
; CHECK: RET
entry:
  %cmp = icmp eq i8 %a, %b
  ret i1 %cmp
}

define i1 @ret_i1_ult(i16 %a, i16 %b) {
; CHECK-LABEL: ret_i1_ult:
; CHECK-NOT: SET_
; CHECK: RET
entry:
  %cmp = icmp ult i16 %a, %b
  ret i1 %cmp
}
