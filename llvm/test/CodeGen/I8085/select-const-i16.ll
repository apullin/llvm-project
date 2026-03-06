; RUN: llc -mattr=i8085,sram < %s -march=i8085 | FileCheck %s

; Select with constant operands.
; Adapted from RISC-V select-const.ll

define i16 @select_const_one_zero(i1 %cond) {
; CHECK-LABEL: select_const_one_zero:
; CHECK: ANA E
; CHECK: ANA D
; CHECK: RET
entry:
  %sel = select i1 %cond, i16 1, i16 0
  ret i16 %sel
}

define i16 @select_const_one_away(i1 %cond) {
; CHECK-LABEL: select_const_one_away:
; CHECK: ANI 1
; CHECK: LXI D, 4
; CHECK: LXI B, 3
; CHECK: JNZ
; CHECK: RET
entry:
  %sel = select i1 %cond, i16 3, i16 4
  ret i16 %sel
}

define i16 @select_const_arbitrary(i1 %cond) {
; CHECK-LABEL: select_const_arbitrary:
; CHECK: ANI 1
; CHECK: LXI D, 1000
; CHECK: LXI B, 42
; CHECK: JNZ
; CHECK: RET
entry:
  %sel = select i1 %cond, i16 42, i16 1000
  ret i16 %sel
}

define i8 @select_const_i8(i1 %cond) {
; CHECK-LABEL: select_const_i8:
; CHECK: ANI 1
; CHECK: JNZ
; CHECK: MVI A, 20
; CHECK: RET
; CHECK: MVI A, 10
; CHECK: RET
entry:
  %sel = select i1 %cond, i8 10, i8 20
  ret i8 %sel
}

define i16 @select_const_zero_nonzero(i1 %cond) {
; CHECK-LABEL: select_const_zero_nonzero:
; CHECK: ANI 1
; CHECK: LXI D, 255
; CHECK: LXI B, 0
; CHECK: JNZ
; CHECK: RET
entry:
  %sel = select i1 %cond, i16 0, i16 255
  ret i16 %sel
}
