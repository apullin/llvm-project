; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; i8 equality select

define i8 @cmp_eq_i8(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_eq_i8:
; CHECK: PUSH D
; CHECK: LXI H, 5
; CHECK: DAD SP
; CHECK: MOV B, H
; CHECK: MOV C, L
; CHECK: LDAX B
; CHECK: MOV B, A
; CHECK: LXI H, 4
; CHECK: DAD SP
; CHECK: MOV D, H
; CHECK: MOV E, L
; CHECK: LDAX D
; CHECK: MOV C, A
; CHECK: MOV A, C
; CHECK: SUB B
; CHECK: JZ
; CHECK: MVI B, 0
; CHECK: JMP
; CHECK: MVI B, 1
; CHECK: MOV A, B
; CHECK: POP D
; CHECK: RET

  %cmp = icmp eq i8 %a, %b
  %sel = select i1 %cmp, i8 1, i8 0
  ret i8 %sel
}

; i8 unsigned less-than select

define i8 @cmp_ult_i8(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_ult_i8:
; CHECK: PUSH D
; CHECK: LXI H, 5
; CHECK: DAD SP
; CHECK: MOV B, H
; CHECK: MOV C, L
; CHECK: LDAX B
; CHECK: MOV B, A
; CHECK: LXI H, 4
; CHECK: DAD SP
; CHECK: MOV D, H
; CHECK: MOV E, L
; CHECK: LDAX D
; CHECK: MOV C, A
; CHECK: MOV A, C
; CHECK: SUB B
; CHECK: JC
; CHECK: MVI B, 0
; CHECK: JMP
; CHECK: MVI B, 1
; CHECK: MOV A, B
; CHECK: POP D
; CHECK: RET

  %cmp = icmp ult i8 %a, %b
  %sel = select i1 %cmp, i8 1, i8 0
  ret i8 %sel
}

; i16 equality select

define i8 @cmp_eq_i16(i16 %a, i16 %b) {
; CHECK-LABEL: cmp_eq_i16:
; CHECK: PUSH D
; CHECK: LXI H, 65532
; CHECK: DAD SP
; CHECK: SPHL
; CHECK: LXI H, 10
; CHECK: DAD SP
; CHECK: MOV B, H
; CHECK: MOV C, L
; CHECK: MOV H, B
; CHECK: MOV L, C
; CHECK: MOV C, M
; CHECK: INX H
; CHECK: MOV B, M
; CHECK: LXI H, 3
; CHECK: DAD SP
; CHECK: MOV M, B
; CHECK: LXI H, 2
; CHECK: DAD SP
; CHECK: MOV M, C
; CHECK: LXI H, 8
; CHECK: DAD SP
; CHECK: MOV B, H
; CHECK: MOV C, L
; CHECK: MOV H, B
; CHECK: MOV L, C
; CHECK: MOV C, M
; CHECK: INX H
; CHECK: MOV B, M
; CHECK: LXI H, 1
; CHECK: DAD SP
; CHECK: MOV M, B
; CHECK: LXI H, 0
; CHECK: DAD SP
; CHECK: MOV M, C
; CHECK: MVI B, 1
; CHECK: LXI H, 3
; CHECK: DAD SP
; CHECK: MOV D, M
; CHECK: LXI H, 2
; CHECK: DAD SP
; CHECK: MOV E, M
; CHECK: LXI H, 0
; CHECK: DAD SP
; CHECK: MOV A, M
; CHECK: INX H
; CHECK: MOV H, M
; CHECK: MOV L, A
; CHECK: MOV A, H
; CHECK: CMP D
; CHECK: JNZ
; CHECK: MOV A, L
; CHECK: CMP E
; CHECK: JNZ
; CHECK: JMP
; CHECK: MVI B, 0
; CHECK: MOV A, B
; CHECK: LXI H, 4
; CHECK: DAD SP
; CHECK: SPHL
; CHECK: POP D
; CHECK: RET

  %cmp = icmp eq i16 %a, %b
  %sel = select i1 %cmp, i8 1, i8 0
  ret i8 %sel
}

; i16 unsigned less-than select

define i8 @cmp_ult_i16(i16 %a, i16 %b) {
; CHECK-LABEL: cmp_ult_i16:
; CHECK: PUSH D
; CHECK: LXI H, 6
; CHECK: DAD SP
; CHECK: MOV B, H
; CHECK: MOV C, L
; CHECK: MOV H, B
; CHECK: MOV L, C
; CHECK: MOV C, M
; CHECK: INX H
; CHECK: MOV B, M
; CHECK: LXI H, 4
; CHECK: DAD SP
; CHECK: MOV D, H
; CHECK: MOV E, L
; CHECK: MOV H, D
; CHECK: MOV L, E
; CHECK: MOV E, M
; CHECK: INX H
; CHECK: MOV D, M
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
; CHECK: JMP
; CHECK: MVI B, 0
; CHECK: MOV A, B
; CHECK: POP D
; CHECK: RET

  %cmp = icmp ult i16 %a, %b
  %sel = select i1 %cmp, i8 1, i8 0
  ret i8 %sel
}
