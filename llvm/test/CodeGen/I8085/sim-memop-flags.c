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
// RUN:   -d 0x0608:0x06 %t.bin 2>&1 | FileCheck %s --check-prefix=MEMOP

__attribute__((naked)) int main(void) {
  __asm__ volatile("lxi h, 0x0600\n"
                   "mvi m, 0x01\n"
                   "mvi a, 0x0f\n"
                   "add m\n"
                   "lxi sp, 0x0620\n"
                   "push psw\n"
                   "lda 0x061f\n"
                   "sta 0x0608\n"
                   "lda 0x061e\n"
                   "sta 0x0609\n"
                   "mvi m, 0x00\n"
                   "mvi a, 0x00\n"
                   "stc\n"
                   "sbb m\n"
                   "lxi sp, 0x0620\n"
                   "push psw\n"
                   "lda 0x061f\n"
                   "sta 0x060a\n"
                   "lda 0x061e\n"
                   "sta 0x060b\n"
                   "mvi m, 0xf0\n"
                   "mvi a, 0x0f\n"
                   "ana m\n"
                   "lxi sp, 0x0620\n"
                   "push psw\n"
                   "lda 0x061f\n"
                   "sta 0x060c\n"
                   "lda 0x061e\n"
                   "sta 0x060d\n"
                   "hlt\n");
}

// MEMOP: Memory dump 0x0608 - 0x060D (6 bytes):
// MEMOP: 0608: 10 10 FF 95 00 54
// MEMOP: "halt":"hlt"
