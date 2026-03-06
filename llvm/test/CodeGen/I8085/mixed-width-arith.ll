; RUN: llc -mattr=i8085,sram < %s -march=i8085 | FileCheck %s

; Mixed-width arithmetic: operations crossing i8/i16/i32 boundaries.
; Adapted from RISC-V sext-zext-trunc.ll patterns.

; i8 + i8 -> i16 (zero-extended addition)
define i16 @add_i8_to_i16_zext(i8 %a, i8 %b) {
; CHECK-LABEL: add_i8_to_i16_zext:
; CHECK: MVI D, 0
; CHECK: MVI B, 0
; CHECK: ADD E
; CHECK: ADC D
; CHECK: RET
entry:
  %a16 = zext i8 %a to i16
  %b16 = zext i8 %b to i16
  %sum = add i16 %a16, %b16
  ret i16 %sum
}

; i8 + i8 -> i16 (sign-extended addition)
define i16 @add_i8_to_i16_sext(i8 %a, i8 %b) {
; CHECK-LABEL: add_i8_to_i16_sext:
; CHECK: ADI 128
; CHECK: SBB A
; CHECK: ADD E
; CHECK: ADC D
; CHECK: RET
entry:
  %a16 = sext i8 %a to i16
  %b16 = sext i8 %b to i16
  %sum = add i16 %a16, %b16
  ret i16 %sum
}

; i16 * i16 -> i32 (widening multiply calls __mulsi16)
define i32 @mul_i16_to_i32(i16 %a, i16 %b) {
; CHECK-LABEL: mul_i16_to_i32:
; CHECK: CALL __mulsi16
; CHECK: RET
entry:
  %a32 = sext i16 %a to i32
  %b32 = sext i16 %b to i32
  %prod = mul i32 %a32, %b32
  ret i32 %prod
}

; i8 -> i32 via chain: zext i8 to i16, then zext i16 to i32
define i32 @chain_zext_i8_i32(i8 %a) {
; CHECK-LABEL: chain_zext_i8_i32:
; CHECK: MVI B, 0
; CHECK: MVI M, 0
; CHECK: RET
entry:
  %a16 = zext i8 %a to i16
  %a32 = zext i16 %a16 to i32
  ret i32 %a32
}

; Truncate i32 -> i8 (just loads low byte)
define i8 @trunc_i32_to_i8(i32 %a) {
; CHECK-LABEL: trunc_i32_to_i8:
; CHECK: LXI H, 2
; CHECK: DAD SP
; CHECK: MOV A, M
; CHECK: RET
entry:
  %t = trunc i32 %a to i8
  ret i8 %t
}

; Mixed: (i8 zext i16) + i16 -> truncate to i8
define i8 @mixed_add_trunc(i8 %a, i16 %b) {
; CHECK-LABEL: mixed_add_trunc:
; CHECK: MVI B, 0
; CHECK: ADD E
; CHECK: ADC D
; CHECK: MOV A, C
; CHECK: RET
entry:
  %a16 = zext i8 %a to i16
  %sum = add i16 %a16, %b
  %t = trunc i16 %sum to i8
  ret i8 %t
}
