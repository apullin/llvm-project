# REQUIRES: i8085
# RUN: llvm-mc -triple=i8085-unknown-elf -filetype=obj %s -o %t.o
# RUN: llvm-mc -triple=i8085-unknown-elf -filetype=obj %S/Inputs/i8085-ext.s -o %t2.o
# RUN: ld.lld -m i8085elf --section-start=.data=0x2000 %t2.o %t.o -o %t
# RUN: llvm-objdump -s -j .data %t | FileCheck %s

# CHECK: Contents of section .data:
# CHECK: 34120020

  .text
  .globl _start
_start:
  NOP

  .data
  .globl ref
ref:
  .word ext
