; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

define i16 @add_sub_1(i16,i16) {
; CHECK-LABEL: add_sub_1:
; CHECK: PUSH D
; CHECK: LXI H, 6
; CHECK: DAD SP
; CHECK: MOV B, H
; CHECK: MOV C, L
; CHECK: LDAX B
; CHECK: MOV E, A
; CHECK: INX B
; CHECK: LDAX B
; CHECK: MOV D, A
; CHECK: DCX B
; CHECK: LXI H, 4
; CHECK: DAD SP
; CHECK: MOV B, H
; CHECK: MOV C, L
; CHECK: MOV H, B
; CHECK: MOV L, C
; CHECK: MOV C, M
; CHECK: INX H
; CHECK: MOV B, M
; CHECK: MOV A, C
; CHECK: ADD E
; CHECK: MOV C, A
; CHECK: MOV A, B
; CHECK: ADC D
; CHECK: MOV B, A
; CHECK: MOV A, C
; CHECK: ADD C
; CHECK: MOV C, A
; CHECK: MOV A, B
; CHECK: ADC B
; CHECK: MOV B, A
; CHECK: MOV A, C
; CHECK: ADD C
; CHECK: MOV C, A
; CHECK: MOV A, B
; CHECK: ADC B
; CHECK: MOV B, A
; CHECK: MOV A, C
; CHECK: ADD C
; CHECK: MOV C, A
; CHECK: MOV A, B
; CHECK: ADC B
; CHECK: MOV B, A
; CHECK: LXI H, 223
; CHECK: MOV A, C
; CHECK: SUB L
; CHECK: MOV C, A
; CHECK: MOV A, B
; CHECK: SBB H
; CHECK: MOV B, A
; CHECK: POP D
; CHECK: RET

  %3 = add i16 %0, %1
  %4 = add i16 %3, %3
  %5 = add i16 %4, %4
  %6 = add i16 %5, %5
  %7 = sub i16 %6, 223
  ret i16 %7
}


define i16 @add_sub_2(i16,i16) {
; CHECK-LABEL: add_sub_2:
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
; CHECK: ADD C
; CHECK: MOV E, A
; CHECK: MOV A, D
; CHECK: ADC B
; CHECK: MOV D, A
; CHECK: LXI B, 10000
; CHECK: MOV A, E
; CHECK: SUB C
; CHECK: MOV E, A
; CHECK: MOV A, D
; CHECK: SBB B
; CHECK: MOV D, A
; CHECK: MOV B, D
; CHECK: MOV C, E
; CHECK: MOV A, C
; CHECK: ADD C
; CHECK: MOV C, A
; CHECK: MOV A, B
; CHECK: ADC B
; CHECK: MOV B, A
; CHECK: LXI H, 3
; CHECK: MOV A, C
; CHECK: ADD L
; CHECK: MOV C, A
; CHECK: MOV A, B
; CHECK: ADC H
; CHECK: MOV B, A
; CHECK: POP D
; CHECK: RET

  %3 = add i16 %0, %1
  %4 = sub i16 %3, 10000
  %5 = add i16 %4, %4
  %6 = add i16 3, %5
  ret i16 %6
}

define i8 @add_sub_3(i8,i8) {
; CHECK-LABEL: add_sub_3:
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
; CHECK: ADD B
; CHECK: MOV C, A
; CHECK: MVI B, 58
; CHECK: MOV A, C
; CHECK: ADD B
; CHECK: MOV C, A
; CHECK: MOV A, C
; CHECK: POP D
; CHECK: RET

  %3 = add i8 %0, %1 ; 48
  %4 = sub i8 100, %3 ; 52
  %5 = sub i8 54, %4 ; 2
  %6 = add i8 101, %5 ; 103
  %7 = add i8 3, %6 ;  106
  ret i8 %7
}


define i8 @add_sub_4(i8,i8) {
; CHECK-LABEL: add_sub_4:
; CHECK: LXI H, 65534
; CHECK: DAD SP
; CHECK: SPHL
; CHECK: MVI B, 100
; CHECK: LXI H, 1
; CHECK: DAD SP
; CHECK: MOV M, B
; CHECK: LXI H, 4
; CHECK: DAD SP
; CHECK: MOV B, H
; CHECK: MOV C, L
; CHECK: LDAX B
; CHECK: MOV B, A
; CHECK: MVI C, -95
; CHECK: MOV A, C
; CHECK: SUB B
; CHECK: MOV C, A
; CHECK: LXI H, 0
; CHECK: DAD SP
; CHECK: MOV M, C
; CHECK: MOV A, C
; CHECK: LXI H, 2
; CHECK: DAD SP
; CHECK: SPHL
; CHECK: RET

  %3 = alloca i8, align 1
  store i8 100, i8* %3, align 1
  %4 = load i8, i8* %3, align 1

  %5 = alloca i8, align 1
  store i8 %0, i8* %5, align 1
  %6 = load i8, i8* %5, align 1

  %7 = add i8 %4, %6
  %8 = sub i8 5 , %7

  %9 = alloca i8, align 1
  store i8 %8, i8* %9, align 1
  %10 = load i8, i8* %9, align 1

  ret i8 %10
}
