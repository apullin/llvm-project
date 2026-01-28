; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; i8 equality select

define i8 @cmp_eq_i8(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_eq_i8:
; CHECK: MOV A, C
; CHECK: SUB B
; CHECK: JZ
; CHECK: MVI B, 0
; CHECK: MOV A, B
; CHECK: RET
; CHECK: MVI B, 1
; CHECK: MOV A, B
; CHECK: RET
  %cmp = icmp eq i8 %a, %b
  %sel = select i1 %cmp, i8 1, i8 0
  ret i8 %sel
}

; i8 unsigned less-than select

define i8 @cmp_ult_i8(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_ult_i8:
; CHECK: MOV A, C
; CHECK: SUB B
; CHECK: JC
; CHECK: MVI B, 0
; CHECK: MOV A, B
; CHECK: RET
; CHECK: MVI B, 1
; CHECK: MOV A, B
; CHECK: RET
  %cmp = icmp ult i8 %a, %b
  %sel = select i1 %cmp, i8 1, i8 0
  ret i8 %sel
}

; i16 equality select

define i8 @cmp_eq_i16(i16 %a, i16 %b) {
; CHECK-LABEL: cmp_eq_i16:
; CHECK: CMP D
; CHECK: JNZ
; CHECK: MOV A, C
; CHECK: CMP E
; CHECK: JNZ
  %cmp = icmp eq i16 %a, %b
  %sel = select i1 %cmp, i8 1, i8 0
  ret i8 %sel
}

; i16 unsigned less-than select

define i8 @cmp_ult_i16(i16 %a, i16 %b) {
; CHECK-LABEL: cmp_ult_i16:
; CHECK: MOV A, E
; CHECK: SUB C
; CHECK: MOV E, A
; CHECK: MOV A, D
; CHECK: SBB B
; CHECK: MOV D, A
; CHECK: MOV B, D
; CHECK: MOV C, E
; CHECK: JNC
; CHECK: MVI B, 1
; CHECK: MVI B, 0
; CHECK: MOV A, B
; CHECK: RET
  %cmp = icmp ult i16 %a, %b
  %sel = select i1 %cmp, i8 1, i8 0
  ret i8 %sel
}
