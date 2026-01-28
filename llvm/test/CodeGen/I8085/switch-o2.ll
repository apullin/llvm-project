; RUN: llc -O2 -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Ensure large switches stay as compare chains at -O2 (no jump table crashes).

define i8 @switch_o2(i8 %x) {
; CHECK-LABEL: switch_o2:
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
    i8 8, label %c8
    i8 9, label %c9
  ]

c0: ret i8 10
c1: ret i8 11
c2: ret i8 12
c3: ret i8 13
c4: ret i8 14
c5: ret i8 15
c6: ret i8 16
c7: ret i8 17
c8: ret i8 18
c9: ret i8 19

default:
  ret i8 0
}
