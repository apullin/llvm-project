; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Larger switch to exercise compare chains.

define i8 @switch_many(i8 %x) {
; CHECK-LABEL: switch_many:
; CHECK: JZ
; CHECK: JMP
; CHECK: RET
entry:
  switch i8 %x, label %default [
    i8 0, label %c0
    i8 1, label %c1
    i8 2, label %c2
    i8 3, label %c3
    i8 4, label %c4
    i8 5, label %c5
    i8 6, label %c6
    i8 7, label %c7
  ]

c0: ret i8 10
c1: ret i8 11
c2: ret i8 12
c3: ret i8 13
c4: ret i8 14
c5: ret i8 15
c6: ret i8 16
c7: ret i8 17

default:
  ret i8 0
}
