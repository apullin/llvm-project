  .section .vectors,"ax"
  .globl g_pfnVectors
  .globl Reset_Handler
  .globl Default_Handler

g_pfnVectors:
  .org 0x0000
  JMP Reset_Handler
  .org 0x0008
  JMP RST1_Handler
  .org 0x0010
  JMP RST2_Handler
  .org 0x0018
  JMP RST3_Handler
  .org 0x0020
  JMP RST4_Handler
  .org 0x0024
  JMP TRAP_Handler
  .org 0x0028
  JMP RST5_Handler
  .org 0x002C
  JMP RST5_5_Handler
  .org 0x0030
  JMP RST6_Handler
  .org 0x0034
  JMP RST6_5_Handler
  .org 0x0038
  JMP RST7_Handler
  .org 0x003C
  JMP RST7_5_Handler

  .text
  .globl _start
  .type _start,@function
Reset_Handler:
_start:
  LXI SP, _stack

  LXI H, _sidata
  LXI D, _sdata
  LXI B, _edata

  MOV A, D
  CMP B
  JNZ data_loop
  MOV A, E
  CMP C
  JZ data_done

data_loop:
  MOV A, M
  STAX D
  INX H
  INX D
  MOV A, D
  CMP B
  JNZ data_loop
  MOV A, E
  CMP C
  JNZ data_loop

data_done:
  LXI H, _sbss
  LXI D, _ebss
  MVI B, 0

  MOV A, H
  CMP D
  JNZ bss_loop
  MOV A, L
  CMP E
  JZ bss_done

bss_loop:
  MOV M, B
  INX H
  MOV A, H
  CMP D
  JNZ bss_loop
  MOV A, L
  CMP E
  JNZ bss_loop

bss_done:
  CALL main

Default_Handler:
hang:
  JMP hang

  .weak RST1_Handler
  .weak RST2_Handler
  .weak RST3_Handler
  .weak RST4_Handler
  .weak TRAP_Handler
  .weak RST5_Handler
  .weak RST5_5_Handler
  .weak RST6_Handler
  .weak RST6_5_Handler
  .weak RST7_Handler
  .weak RST7_5_Handler

  .set RST1_Handler, Default_Handler
  .set RST2_Handler, Default_Handler
  .set RST3_Handler, Default_Handler
  .set RST4_Handler, Default_Handler
  .set TRAP_Handler, Default_Handler
  .set RST5_Handler, Default_Handler
  .set RST5_5_Handler, Default_Handler
  .set RST6_Handler, Default_Handler
  .set RST6_5_Handler, Default_Handler
  .set RST7_Handler, Default_Handler
  .set RST7_5_Handler, Default_Handler
