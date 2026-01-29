; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Signed i8 less-than select should use sign checks (XRA/ANI 128) and carry logic.

define i8 @cmp_slt_i8(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_slt_i8:
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
; CHECK: XRA B
; CHECK: ANI 128
; CHECK: JZ
; CHECK: JNZ
; CHECK: MOV A, C
; CHECK: SUB B
; CHECK: JNC
; CHECK: MVI B, 1
; CHECK: JMP
; CHECK: MOV A, C
; CHECK: ANI 128
; CHECK: JZ
; CHECK: JNZ
; CHECK: MVI B, 0
; CHECK: MOV A, B
; CHECK: POP D
; CHECK: RET

  %cmp = icmp slt i8 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

; Signed i8 greater-than select should also use sign checks.

define i8 @cmp_sgt_i8(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_sgt_i8:
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
; CHECK: XRA B
; CHECK: ANI 128
; CHECK: JZ
; CHECK: JNZ
; CHECK: MOV A, C
; CHECK: SUB B
; CHECK: MVI B, 1
; CHECK: JZ
; CHECK: JNC
; CHECK: MVI B, 0
; CHECK: JMP
; CHECK: MOV A, C
; CHECK: ANI 128
; CHECK: JZ
; CHECK: JNZ
; CHECK: MVI B, 1
; CHECK: MOV A, B
; CHECK: POP D
; CHECK: RET

  %cmp = icmp sgt i8 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

; Signed i16 less-than select should check sign mismatch then do 16-bit subtract.

define i8 @cmp_slt_i16(i16 %a, i16 %b) {
; CHECK-LABEL: cmp_slt_i16:
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
; CHECK: MOV A, D
; CHECK: XRA B
; CHECK: ANI 128
; CHECK: JNZ
; CHECK: MOV A, E
; CHECK: SUB C
; CHECK: MOV E, A
; CHECK: MOV A, D
; CHECK: SBB B
; CHECK: MOV D, A
; CHECK: MOV B, D
; CHECK: MOV C, E
; CHECK: JC
; CHECK: MVI B, 0
; CHECK: JMP
; CHECK: MOV A, D
; CHECK: ANI 128
; CHECK: JZ
; CHECK: MVI B, 1
; CHECK: MOV A, B
; CHECK: POP D
; CHECK: RET

  %cmp = icmp slt i16 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

; Unsigned i16 greater-or-equal should use carry from 16-bit subtract.

define i8 @cmp_uge_i16(i16 %a, i16 %b) {
; CHECK-LABEL: cmp_uge_i16:
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
; CHECK: MVI B, 0
; CHECK: JMP
; CHECK: MVI B, 1
; CHECK: MOV A, B
; CHECK: POP D
; CHECK: RET

  %cmp = icmp uge i16 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

; Unsigned i8 greater-or-equal should use carry from subtract.

define i8 @cmp_uge_i8(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_uge_i8:
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
; CHECK: JNC
; CHECK: MVI B, 0
; CHECK: JMP
; CHECK: MVI B, 1
; CHECK: MOV A, B
; CHECK: POP D
; CHECK: RET

  %cmp = icmp uge i8 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}
