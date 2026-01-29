; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

@g8 = global i8 0
@g16 = global i16 0

; Global addressing should lower via absolute address materialization.

define void @store_globals(i8 %a, i16 %b) {
; CHECK-LABEL: store_globals:
; CHECK: LXI H, 3
; CHECK: DAD SP
; CHECK: MOV B, H
; CHECK: MOV C, L
; CHECK: MOV H, B
; CHECK: MOV L, C
; CHECK: MOV C, M
; CHECK: INX H
; CHECK: MOV B, M
; CHECK: LXI H, g16
; CHECK: MOV M, C
; CHECK: INX H
; CHECK: MOV M, B
; CHECK: DCX H
; CHECK: LXI H, 2
; CHECK: DAD SP
; CHECK: MOV B, H
; CHECK: MOV C, L
; CHECK: LDAX B
; CHECK: MOV B, A
; CHECK: LXI H, g8
; CHECK: MOV M, B
; CHECK: RET
entry:
  store i8 %a, i8* @g8, align 1
  store i16 %b, i16* @g16, align 1
  ret void
}

define i16 @load_globals() {
; CHECK-LABEL: load_globals:
; CHECK: PUSH D
; CHECK: LXI H, g8
; CHECK: MOV B, M
; CHECK: MOV A, B
; CHECK: MOV C, A
; CHECK: MVI B, 0
; CHECK: LXI H, g16+1
; CHECK: MOV D, M
; CHECK: LXI H, g16
; CHECK: MOV E, M
; CHECK: MOV A, C
; CHECK: ADD E
; CHECK: MOV C, A
; CHECK: MOV A, B
; CHECK: ADC D
; CHECK: MOV B, A
; CHECK: POP D
; CHECK: RET
entry:
  %a = load i8, i8* @g8, align 1
  %b = load i16, i16* @g16, align 1
  %a16 = zext i8 %a to i16
  %sum = add i16 %a16, %b
  ret i16 %sum
}
