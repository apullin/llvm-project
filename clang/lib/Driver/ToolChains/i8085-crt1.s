  .text
  .globl _start
  .type _start,@function
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

hang:
  JMP hang
