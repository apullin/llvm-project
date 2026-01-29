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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 --irq=trap@0 --irq=55@0 -d 0x0300:0x05 %t.bin 2>&1 | FileCheck %s --check-prefix=IRQ

__attribute__((naked)) void isr_trap(void) {
  __asm__ volatile("mvi a, 0xA4\n"
                   "sta 0x0302\n"
                   "ret\n");
}

__attribute__((naked)) void isr_rst55(void) {
  __asm__ volatile("mvi a, 0x55\n"
                   "sta 0x0303\n"
                   "ret\n");
}

__attribute__((naked)) int main(void) {
  __asm__ volatile("di\n"
                   "mvi a, 0x0f\n"
                   "sim\n"
                   "rim\n"
                   "sta 0x0300\n"
                   "ei\n"
                   "nop\n"
                   "rim\n"
                   "sta 0x0301\n"
                   "mvi a, 0x08\n"
                   "sim\n"
                   "nop\n"
                   "rim\n"
                   "sta 0x0304\n"
                   "ret\n");
}

// IRQ: Memory dump 0x0300 - 0x0304 (5 bytes):
// IRQ: 0300: 17 1F A4 55 00
// IRQ: "halt":"hlt"
