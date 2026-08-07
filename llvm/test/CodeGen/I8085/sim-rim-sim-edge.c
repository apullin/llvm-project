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
// RUN:   -d 0x06D0:0x03 %t.bin 2>&1 | FileCheck %s --check-prefix=RIMEDGE

__attribute__((naked)) int main(void) {
  __asm__ volatile("di\n"
                   "mvi a, 0x0f\n"
                   "sim\n"
                   "mvi a, 0x10\n"
                   "sim\n"
                   "rim\n"
                   "sta 0x06d0\n"
                   "mvi a, 0x0f\n"
                   "sim\n"
                   "rim\n"
                   "sta 0x06d1\n"
                   "mvi a, 0x00\n"
                   "sim\n"
                   "rim\n"
                   "sta 0x06d2\n"
                   "hlt\n");
}

// RIMEDGE: Memory dump 0x06D0 - 0x06D2 (3 bytes):
// RIMEDGE: 06D0: 07 07 07
// RIMEDGE: "halt":"hlt"
