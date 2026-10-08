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
// RUN: %i8085_sim -S -q -n 200000 --irq=65@50000 -d 0x0400:0x02 %t.bin 2>&1 | FileCheck %s --check-prefix=HLT

__attribute__((naked)) void isr_rst65(void) {
  __asm__ volatile("mvi a, 0x66\n"
                   "sta 0x0401\n"
                   "ret\n");
}

__attribute__((naked)) int main(void) {
  __asm__ volatile("ei\n"
                   "nop\n"
                   "hlt\n"
                   "mvi a, 0x22\n"
                   "sta 0x0400\n"
                   "ret\n");
}

// HLT: Memory dump 0x0400 - 0x0401 (2 bytes):
// HLT: 0400: 22 66
// HLT: "halt":"hlt"
