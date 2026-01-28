; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Switch lowering for i32 should expand without compare pseudos.

define i32 @switch_i32(i32 %x) {
; CHECK-LABEL: switch_i32:
; CHECK-NOT: JMP_32_IF
; CHECK-NOT: SET_
; CHECK: RET
entry:
  switch i32 %x, label %default [
    i32 0, label %case0
    i32 305419896, label %case1
  ]

case0:
  ret i32 0

case1:
  ret i32 1

default:
  ret i32 2
}
