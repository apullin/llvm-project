; RUN: llc -mattr=i8085,sram < %s -march=i8085 | FileCheck %s

; Sign/zero extension chains across type widths.
; Adapted from RISC-V sext-zext-trunc.ll

; zext i1 -> i16
define i16 @zext_i1_to_i16(i1 %a) {
; CHECK-LABEL: zext_i1_to_i16:
; CHECK: MVI B, 0
; CHECK: ANA E
; CHECK: ANA D
; CHECK: RET
entry:
  %r = zext i1 %a to i16
  ret i16 %r
}

; sext i8 -> i32 (sign extends with ADI 128; SBB A pattern)
define i32 @sext_i8_to_i32(i8 %a) {
; CHECK-LABEL: sext_i8_to_i32:
; CHECK: ADI 128
; CHECK: SBB A
; CHECK: RET
entry:
  %r = sext i8 %a to i32
  ret i32 %r
}

; zext i8 -> i32 (fills upper bytes with zero)
define i32 @zext_i8_to_i32(i8 %a) {
; CHECK-LABEL: zext_i8_to_i32:
; CHECK: MVI B, 0
; CHECK: MVI M, 0
; CHECK: RET
entry:
  %r = zext i8 %a to i32
  ret i32 %r
}

; sext i16 -> i32 (sign extends high byte)
define i32 @sext_i16_to_i32(i16 %a) {
; CHECK-LABEL: sext_i16_to_i32:
; CHECK: ADI 128
; CHECK: SBB A
; CHECK: RET
entry:
  %r = sext i16 %a to i32
  ret i32 %r
}

; zext i16 -> i32 (upper 16 bits zeroed)
define i32 @zext_i16_to_i32(i16 %a) {
; CHECK-LABEL: zext_i16_to_i32:
; CHECK: MVI M, 0
; CHECK: RET
entry:
  %r = zext i16 %a to i32
  ret i32 %r
}

; trunc i32 -> i16 (just keep low 16 bits)
define i16 @trunc_i32_to_i16(i32 %a) {
; CHECK-LABEL: trunc_i32_to_i16:
; CHECK: LXI H, 3
; CHECK: DAD SP
; CHECK: MOV B, M
; CHECK: DCX H
; CHECK: MOV C, M
; CHECK: RET
entry:
  %r = trunc i32 %a to i16
  ret i16 %r
}

; trunc i16 -> i1 (just low bit)
define i1 @trunc_i16_to_i1(i16 %a) {
; CHECK-LABEL: trunc_i16_to_i1:
; CHECK: LXI H, 2
; CHECK: DAD SP
; CHECK: MOV A, M
; CHECK: RET
entry:
  %r = trunc i16 %a to i1
  ret i1 %r
}
