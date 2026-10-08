// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %i8085_llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 200000 \
// RUN:   -d 0x0520:0x06 %t.bin 2>&1 | FileCheck %s --check-prefix=DAD

__attribute__((naked)) int main(void) {
  __asm__ volatile("lxi h, 0xffff\n"
                   "lxi b, 0x0001\n"
                   "dad b\n"
                   "mvi a, 0x00\n"
                   "adc a\n"
                   "sta 0x0520\n"
                   "shld 0x0521\n"
                   "lxi h, 0x1234\n"
                   "lxi b, 0x1111\n"
                   "dad b\n"
                   "mvi a, 0x00\n"
                   "adc a\n"
                   "sta 0x0523\n"
                   "shld 0x0524\n"
                   "hlt\n");
}

// DAD: Memory dump 0x0520 - 0x0525 (6 bytes):
// DAD: 0520: 01 00 00 00 45 23
// DAD: "halt":"hlt"
