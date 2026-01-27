; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; i32 extend/trunc lowering should not leave 32-bit pseudos in the asm.

define i32 @zext_i8_to_i32(i8 %a) {
; CHECK-LABEL: zext_i8_to_i32:
; CHECK-NOT: ZEXT8TO32
; CHECK: RET
entry:
  %z = zext i8 %a to i32
  ret i32 %z
}

define i32 @sext_i16_to_i32(i16 %a) {
; CHECK-LABEL: sext_i16_to_i32:
; CHECK-NOT: AEXT16TO32
; CHECK: RET
entry:
  %s = sext i16 %a to i32
  ret i32 %s
}

define i16 @trunc_i32_to_i16(i32 %a) {
; CHECK-LABEL: trunc_i32_to_i16:
; CHECK-NOT: TRUNC32TO16
; CHECK: RET
entry:
  %t = trunc i32 %a to i16
  ret i16 %t
}

define i8 @trunc_i32_to_i8(i32 %a) {
; CHECK-LABEL: trunc_i32_to_i8:
; CHECK-NOT: TRUNC32TO8
; CHECK: RET
entry:
  %t = trunc i32 %a to i8
  ret i8 %t
}
