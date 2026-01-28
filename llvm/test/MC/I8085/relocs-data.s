# RUN: llvm-mc -triple=i8085 -filetype=obj < %s | llvm-objdump -r - | FileCheck %s

  .data
  .globl g16
  .globl g8
  g16:
    .word ext
  g8:
    .byte ext

  .text
  .globl _start
_start:
  lxi h, ext
  shld g16
  call ext

# CHECK: RELOCATION RECORDS FOR [.text]:
# CHECK: R_I8085_16{{.*}}ext
# CHECK: R_I8085_16{{.*}}g16
# CHECK: R_I8085_16{{.*}}ext
# CHECK: RELOCATION RECORDS FOR [.data]:
# CHECK: R_I8085_16{{.*}}ext
# CHECK: R_I8085_8{{.*}}ext
