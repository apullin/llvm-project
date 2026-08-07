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
// RUN:   -d 0x0580:0x04 %t.bin 2>&1 | FileCheck %s --check-prefix=CMC

__attribute__((naked)) int main(void) {
  __asm__ volatile("xra a\n"
                   "stc\n"
                   "cmc\n"
                   "mvi a, 0x00\n"
                   "adc a\n"
                   "sta 0x0580\n"
                   "stc\n"
                   "mvi a, 0x00\n"
                   "adc a\n"
                   "sta 0x0581\n"
                   "xra a\n"
                   "stc\n"
                   "cma\n"
                   "jz cma_z_ok\n"
                   "mvi a, 0xee\n"
                   "jmp cma_z_store\n"
                   "cma_z_ok:\n"
                   "mvi a, 0x01\n"
                   "cma_z_store:\n"
                   "sta 0x0582\n"
                   "mvi a, 0x00\n"
                   "adc a\n"
                   "sta 0x0583\n"
                   "hlt\n");
}

// CMC: Memory dump 0x0580 - 0x0583 (4 bytes):
// CMC: 0580: 00 01 01 01
// CMC: "halt":"hlt"
