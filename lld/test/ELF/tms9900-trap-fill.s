# REQUIRES: tms9900

# RUN: rm -rf %t && split-file %s %t
# RUN: llvm-mc -triple tms9900 -filetype=obj %t/a.s -o %t/a.o
# RUN: llvm-mc -triple tms9900 -filetype=obj %t/b.s -o %t/b.o
# RUN: ld.lld -T %t/script.ld %t/a.o %t/b.o -o %t/out
# RUN: llvm-objdump -s -j .text %t/out | FileCheck %s

## Executable-section padding uses BLWP @0 as a trap. The symbolic-addressing
## bit is part of the first instruction word: 0x0420, not 0x0400 (BLWP R0).
# CHECK:      Contents of section .text:
# CHECK-NEXT:  0000 10001000 04200000 10000420

#--- a.s
  .section .text.a,"ax",@progbits
  .globl _start
_start:
  nop
  nop

#--- b.s
  .section .text.b,"ax",@progbits
  .p2align 3
target:
  nop

#--- script.ld
SECTIONS {
  .text 0 : {
    *(.text.a)
    *(.text.b)
  }
}
