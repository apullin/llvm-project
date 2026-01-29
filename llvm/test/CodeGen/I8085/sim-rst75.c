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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 --irq=75@0 --irq=75@50000 -d 0x0310:0x03 %t.bin 2>&1 | FileCheck %s --check-prefix=RST75

__attribute__((naked)) void isr_rst75(void) {
  __asm__ volatile("mvi a, 0x75\n"
                   "sta 0x0312\n"
                   "reti\n");
}

__attribute__((naked)) int main(void) {
  __asm__ volatile("di\n"
                   "mvi a, 0x0c\n"
                   "sim\n"
                   "rim\n"
                   "sta 0x0310\n"
                   "mvi a, 0x1c\n"
                   "sim\n"
                   "rim\n"
                   "sta 0x0311\n"
                   "mvi a, 0x08\n"
                   "sim\n"
                   "ei\n"
                   "nop\n"
                   "ret\n");
}

// RST75: Memory dump 0x0310 - 0x0312 (3 bytes):
// RST75: 0310: 44 04 75
// RST75: "halt":"hlt"
