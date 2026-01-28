; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Pointer compares should lower without leaving compare/branch pseudos.

define i8 @ptr_eq(ptr %a, ptr %b) {
; CHECK-LABEL: ptr_eq:
; CHECK-NOT: SET_
; CHECK-NOT: JMP_16_IF
; CHECK: RET
entry:
  %cmp = icmp eq ptr %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

define i8 @ptr_ult(ptr %a, ptr %b) {
; CHECK-LABEL: ptr_ult:
; CHECK-NOT: SET_
; CHECK-NOT: JMP_16_IF
; CHECK: RET
entry:
  %cmp = icmp ult ptr %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}
