; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; i32 select should lower without leaving SELECT_32 pseudos.

define i32 @select_i32(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: select_i32:
; CHECK-NOT: SELECT_32
; CHECK-NOT: SET_
; CHECK: RET
entry:
  %cmp = icmp ult i32 %a, %b
  %sel = select i1 %cmp, i32 %a, i32 %c
  ret i32 %sel
}
