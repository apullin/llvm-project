; RUN: llc -O2 -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s
; RUN: llc -O2 -mattr=i8085,sram < %s -march=i8085 -filetype=obj -o %t
; RUN: llvm-nm -a %t | FileCheck %s --check-prefix=OBJ

; Large sparse switches should lower to a jump table plus indirect branch.

declare void @sink(i8)

define i16 @switch_jumptable(i8 %x) {
; CHECK-LABEL: switch_jumptable:
; CHECK: LXI B, JTI0_0
; CHECK: PCHL
; CHECK: JTI0_0:
; OBJ: JTI0_0
; OBJ-NOT: CPI
entry:
  switch i8 %x, label %default [
    i8 0, label %c0
    i8 2, label %c2
    i8 4, label %c4
    i8 6, label %c6
    i8 8, label %c8
    i8 10, label %c10
    i8 12, label %c12
    i8 14, label %c14
    i8 16, label %c16
    i8 18, label %c18
    i8 20, label %c20
    i8 22, label %c22
    i8 24, label %c24
    i8 26, label %c26
    i8 28, label %c28
    i8 30, label %c30
  ]

c0:
  call void @sink(i8 1)
  ret i16 1
c2:
  call void @sink(i8 5)
  ret i16 5
c4:
  call void @sink(i8 7)
  ret i16 7
c6:
  call void @sink(i8 11)
  ret i16 11
c8:
  call void @sink(i8 13)
  ret i16 13
c10:
  call void @sink(i8 17)
  ret i16 17
c12:
  call void @sink(i8 19)
  ret i16 19
c14:
  call void @sink(i8 23)
  ret i16 23
c16:
  call void @sink(i8 29)
  ret i16 29
c18:
  call void @sink(i8 31)
  ret i16 31
c20:
  call void @sink(i8 37)
  ret i16 37
c22:
  call void @sink(i8 41)
  ret i16 41
c24:
  call void @sink(i8 43)
  ret i16 43
c26:
  call void @sink(i8 47)
  ret i16 47
c28:
  call void @sink(i8 53)
  ret i16 53
c30:
  call void @sink(i8 59)
  ret i16 59
default:
  call void @sink(i8 3)
  ret i16 3
}
