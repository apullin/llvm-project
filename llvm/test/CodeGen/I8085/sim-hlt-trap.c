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
// RUN:   --irq=45@50000 -d 0x0410:0x02 %t.bin 2>&1 | FileCheck %s --check-prefix=TRAP

__attribute__((naked)) void isr_trap(void) {
  __asm__ volatile("mvi a, 0xA4\n"
                   "sta 0x0411\n"
                   "ret\n");
}

__attribute__((naked)) int main(void) {
  __asm__ volatile("di\n"
                   "hlt\n"
                   "mvi a, 0x11\n"
                   "sta 0x0410\n"
                   "ret\n");
}

// TRAP: Memory dump 0x0410 - 0x0411 (2 bytes):
// TRAP: 0410: 11 A4
// TRAP: "halt":"hlt"
