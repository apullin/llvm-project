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
// RUN:   --irq=55@50000 --irq=65@50010 -d 0x0342:0x02 %t.bin 2>&1 | FileCheck %s --check-prefix=RET

__attribute__((naked)) void isr_rst55(void) {
  __asm__ volatile("mvi a, 0x55\n"
                   "sta 0x0342\n"
                   "ret\n");
}

__attribute__((naked)) void isr_rst65(void) {
  __asm__ volatile("mvi a, 0x66\n"
                   "sta 0x0343\n"
                   "ret\n");
}

__attribute__((naked)) int main(void) {
  __asm__ volatile("ei\n"
                   "hlt\n"
                   "ret\n");
}

// RET: Memory dump 0x0342 - 0x0343 (2 bytes):
// RET: 0342: 55 00
// RET: "halt":"hlt"
