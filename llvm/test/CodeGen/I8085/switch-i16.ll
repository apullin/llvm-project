; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Switch lowering for i16 should expand to compare/branch chain.

define i16 @switch_i16(i16 %x) {
; CHECK-LABEL: switch_i16:
; CHECK-COUNT-2: J{{N?}}Z
; CHECK: RET
entry:
  switch i16 %x, label %default [
    i16 0, label %case0
    i16 258, label %case258
  ]

case0:
  ret i16 0

case258:
  ret i16 258

default:
  ret i16 1
}
