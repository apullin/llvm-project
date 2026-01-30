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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 \
// RUN:   --irq=55@50000 --irq=65@50010 -d 0x0342:0x02 %t.bin 2>&1 | FileCheck %s --check-prefix=RETI

__attribute__((naked)) void isr_rst55(void) {
  __asm__ volatile("mvi a, 0x55\n"
                   "sta 0x0342\n"
                   "reti\n");
}

__attribute__((naked)) void isr_rst65(void) {
  __asm__ volatile("mvi a, 0x66\n"
                   "sta 0x0343\n"
                   "reti\n");
}

__attribute__((naked)) int main(void) {
  __asm__ volatile("ei\n"
                   "hlt\n"
                   "ret\n");
}

// RETI: Memory dump 0x0342 - 0x0343 (2 bytes):
// RETI: 0342: 55 00
// RETI: "halt":"hlt"
