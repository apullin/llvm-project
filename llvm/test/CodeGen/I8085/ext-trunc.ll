; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

define i16 @zext_i8(i8 %a) {
; CHECK-LABEL: zext_i8:
; CHECK: MOV B, M
; CHECK: MOV C, A
; CHECK: MVI B, 0
; CHECK: RET
  %z = zext i8 %a to i16
  ret i16 %z
}

define i16 @sext_i8(i8 %a) {
; CHECK-LABEL: sext_i8:
; CHECK: MOV B, M
; CHECK: MOV C, A
; CHECK: ADI 128
; CHECK: SBB A
; CHECK: MOV B, A
; CHECK: RET
  %s = sext i8 %a to i16
  ret i16 %s
}

define i8 @trunc_i16(i16 %a) {
; CHECK-LABEL: trunc_i16:
; CHECK: MOV A, M
; CHECK: RET
  %t = trunc i16 %a to i8
  ret i8 %t
}
