  .section .vectors, "ax"
  .globl __vector_table
__vector_table:
  .org 0x0000
  JMP _start
  .org 0x0008
  JMP isr_rst1
  .org 0x0010
  JMP isr_rst2
  .org 0x0018
  JMP isr_rst3
  .org 0x0020
  JMP isr_rst4
  .org 0x0024
  JMP isr_trap
  .org 0x0028
  JMP isr_rst5
  .org 0x002C
  JMP isr_rst55
  .org 0x0030
  JMP isr_rst6
  .org 0x0034
  JMP isr_rst65
  .org 0x0038
  JMP isr_rst7
  .org 0x003C
  JMP isr_rst75

  .section .text.startup, "ax"
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

  .text
  .globl default_isr
default_isr:
  HLT
  JMP default_isr

  .weak isr_rst1
  .weak isr_rst2
  .weak isr_rst3
  .weak isr_rst4
  .weak isr_rst5
  .weak isr_rst6
  .weak isr_rst7
  .weak isr_trap
  .weak isr_rst55
  .weak isr_rst65
  .weak isr_rst75

  .set isr_rst1,  default_isr
  .set isr_rst2,  default_isr
  .set isr_rst3,  default_isr
  .set isr_rst4,  default_isr
  .set isr_rst5,  default_isr
  .set isr_rst6,  default_isr
  .set isr_rst7,  default_isr
  .set isr_trap,  default_isr
  .set isr_rst55, default_isr
  .set isr_rst65, default_isr
  .set isr_rst75, default_isr
