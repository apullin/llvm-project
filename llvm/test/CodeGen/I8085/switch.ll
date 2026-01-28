; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Switch lowering should expand to a compare/branch chain.

define i8 @switch_i8(i8 %x) {
; CHECK-LABEL: switch_i8:
; CHECK: MVI C, 9
; CHECK: SUB C
; CHECK: JZ
; CHECK: MVI C, 5
; CHECK: SUB C
; CHECK: JZ
; CHECK: MVI C, 0
; CHECK: SUB C
; CHECK: JNZ
; CHECK: RET
entry:
  switch i8 %x, label %default [
    i8 0, label %case0
    i8 5, label %case5
    i8 9, label %case9
  ]

case0:
  ret i8 0

case5:
  ret i8 5

case9:
  ret i8 9

default:
  ret i8 1
}
