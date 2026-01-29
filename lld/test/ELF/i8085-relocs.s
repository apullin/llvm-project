# REQUIRES: i8085
# RUN: llvm-mc -triple=i8085-unknown-elf -filetype=obj %s -o %t.o
# RUN: llvm-mc -triple=i8085-unknown-elf -filetype=obj %S/Inputs/i8085-reloc-sym.s -o %t2.o
# RUN: ld.lld -m i8085elf --section-start=.data=0x1234 %t2.o %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

# CHECK: <_start>:
# CHECK: MVI A, 0x34
# CHECK: MVI B, 0x12
# CHECK: MVI C, 0x0
# CHECK: MVI D, 0x0
# CHECK: LXI H, 0x91a
# CHECK: MVI E, 0x1a
# CHECK: MVI L, 0x9
# CHECK: MVI H, 0x0
# CHECK: LXI D, 0x91a
# CHECK: MVI A, 0x1a
# CHECK: MVI B, 0x9

  .extern ext
  .text
  .globl _start
_start:
  mvi a, lo8(ext)
  mvi b, hi8(ext)
  mvi c, hh8(ext)
  mvi d, hhi8(ext)
  lxi h, pm(ext)
  mvi e, pm_lo8(ext)
  mvi l, pm_hi8(ext)
  mvi h, pm_hh8(ext)
  lxi d, gs(ext)
  mvi a, lo8_gs(ext)
  mvi b, hi8_gs(ext)
