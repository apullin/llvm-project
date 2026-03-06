; RUN: llc -mattr=i8085,sram < %s -march=i8085 | FileCheck %s

; Division/remainder by power of 2 patterns — lowered to shifts/masks.
; Adapted from RISC-V div-pow2.ll

; Unsigned division by 2: logical right shift
define i16 @udiv_by_2(i16 %a) {
; CHECK-LABEL: udiv_by_2:
; CHECK: RAR
; CHECK: ANI 127
; CHECK: RET
entry:
  %r = udiv i16 %a, 2
  ret i16 %r
}

; Unsigned division by 4: two logical right shifts
define i16 @udiv_by_4(i16 %a) {
; CHECK-LABEL: udiv_by_4:
; CHECK: RAR
; CHECK: ANI 127
; CHECK: RAR
; CHECK: ANI 127
; CHECK: RET
entry:
  %r = udiv i16 %a, 4
  ret i16 %r
}

; Unsigned division by 256: just high byte -> low byte
define i16 @udiv_by_256(i16 %a) {
; CHECK-LABEL: udiv_by_256:
; CHECK: LXI H, 3
; CHECK: DAD SP
; CHECK: MOV A, M
; CHECK: MOV C, A
; CHECK: MVI B, 0
; CHECK: RET
entry:
  %r = udiv i16 %a, 256
  ret i16 %r
}

; Unsigned remainder by 4: AND 3
define i16 @urem_by_4(i16 %a) {
; CHECK-LABEL: urem_by_4:
; CHECK: LXI D, 3
; CHECK: ANA E
; CHECK: ANA D
; CHECK: RET
entry:
  %r = urem i16 %a, 4
  ret i16 %r
}

; i8 division by 2: single rotate right
define i8 @udiv_i8_by_2(i8 %a) {
; CHECK-LABEL: udiv_i8_by_2:
; CHECK: RRC
; CHECK: ANI 127
; CHECK: RET
entry:
  %r = udiv i8 %a, 2
  ret i8 %r
}

; i8 remainder by 8: AND 7
define i8 @urem_i8_by_8(i8 %a) {
; CHECK-LABEL: urem_i8_by_8:
; CHECK: ANI 7
; CHECK: RET
entry:
  %r = urem i8 %a, 8
  ret i8 %r
}
