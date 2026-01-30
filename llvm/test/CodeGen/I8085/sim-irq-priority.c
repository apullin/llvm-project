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
// RUN:   --irq=45@1000 --irq=75@1000 --irq=65@1000 --irq=55@1000 \
// RUN:   -d 0x0350:0x05 %t.bin 2>&1 | FileCheck %s --check-prefix=PRIO

__attribute__((naked)) void isr_trap(void) {
  __asm__ volatile("lda 0x0354\n"
                   "inr a\n"
                   "sta 0x0354\n"
                   "lxi h, 0x0350\n"
                   "dcr a\n"
                   "mov e, a\n"
                   "mvi d, 0x00\n"
                   "dad d\n"
                   "mvi m, 0xA4\n"
                   "ei\n"
                   "reti\n");
}

__attribute__((naked)) void isr_rst75(void) {
  __asm__ volatile("lda 0x0354\n"
                   "inr a\n"
                   "sta 0x0354\n"
                   "lxi h, 0x0350\n"
                   "dcr a\n"
                   "mov e, a\n"
                   "mvi d, 0x00\n"
                   "dad d\n"
                   "mvi m, 0x75\n"
                   "ei\n"
                   "reti\n");
}

__attribute__((naked)) void isr_rst65(void) {
  __asm__ volatile("lda 0x0354\n"
                   "inr a\n"
                   "sta 0x0354\n"
                   "lxi h, 0x0350\n"
                   "dcr a\n"
                   "mov e, a\n"
                   "mvi d, 0x00\n"
                   "dad d\n"
                   "mvi m, 0x65\n"
                   "ei\n"
                   "reti\n");
}

__attribute__((naked)) void isr_rst55(void) {
  __asm__ volatile("lda 0x0354\n"
                   "inr a\n"
                   "sta 0x0354\n"
                   "lxi h, 0x0350\n"
                   "dcr a\n"
                   "mov e, a\n"
                   "mvi d, 0x00\n"
                   "dad d\n"
                   "mvi m, 0x55\n"
                   "ei\n"
                   "reti\n");
}

__attribute__((naked)) int main(void) {
  __asm__ volatile("di\n"
                   "mvi a, 0x08\n"
                   "sim\n"
                   "xra a\n"
                   "sta 0x0354\n"
                   "ei\n"
                   "wait: hlt\n"
                   "jmp wait\n");
}

// PRIO: Memory dump 0x0350 - 0x0354 (5 bytes):
// PRIO: 0350: A4 75 65 55 04
// PRIO: "halt":"hlt"
