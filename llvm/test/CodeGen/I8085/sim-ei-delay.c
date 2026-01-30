// REQUIRES: i8085-sim
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %S/../../../../../sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o
// RUN: llvm-objcopy -O binary %t.elf %t.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 --irq=55@0 \
// RUN:   -d 0x0360:0x02 %t.bin 2>&1 | FileCheck %s --check-prefix=EIDELAY

__attribute__((naked)) void isr_rst55(void) {
  __asm__ volatile("lda 0x0360\n"
                   "sta 0x0361\n"
                   "reti\n");
}

__attribute__((naked)) int main(void) {
  __asm__ volatile("di\n"
                   "mvi a, 0x08\n"
                   "sim\n"
                   "lxi h, 0x0360\n"
                   "mvi m, 0x00\n"
                   "ei\n"
                   "inr m\n"
                   "hlt\n");
}

// EIDELAY: Memory dump 0x0360 - 0x0361 (2 bytes):
// EIDELAY: 0360: 01 01
// EIDELAY: "halt":"hlt"
