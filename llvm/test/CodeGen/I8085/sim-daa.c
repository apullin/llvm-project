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
// RUN:   -d 0x0380:0x06 %t.bin 2>&1 | FileCheck %s --check-prefix=DAA

__attribute__((naked)) int main(void) {
  __asm__ volatile("mvi a, 0x09\n"
                   "adi 0x09\n"
                   "daa\n"
                   "sta 0x0380\n"
                   "mvi a, 0x00\n"
                   "adc a\n"
                   "sta 0x0381\n"
                   "mvi a, 0x15\n"
                   "adi 0x27\n"
                   "daa\n"
                   "sta 0x0382\n"
                   "mvi a, 0x00\n"
                   "adc a\n"
                   "sta 0x0383\n"
                   "mvi a, 0x99\n"
                   "adi 0x99\n"
                   "daa\n"
                   "sta 0x0384\n"
                   "mvi a, 0x00\n"
                   "adc a\n"
                   "sta 0x0385\n"
                   "hlt\n");
}

// DAA: Memory dump 0x0380 - 0x0385 (6 bytes):
// DAA: 0380: 18 00 42 00 98 01
// DAA: "halt":"hlt"
