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
// RUN:   -d 0x06B0:0x06 %t.bin 2>&1 | FileCheck %s --check-prefix=LMEM

__attribute__((naked)) int main(void) {
  __asm__ volatile("lxi h, 0x0600\n"
                   "mvi m, 0xf0\n"
                   "lxi sp, 0x06C0\n"
                   "mvi a, 0x0f\n"
                   "ana m\n"
                   "push psw\n"
                   "lda 0x06be\n"
                   "sta 0x06b0\n"
                   "lda 0x06bf\n"
                   "sta 0x06b1\n"
                   "mvi m, 0x0f\n"
                   "lxi sp, 0x06C0\n"
                   "mvi a, 0x00\n"
                   "ora m\n"
                   "push psw\n"
                   "lda 0x06be\n"
                   "sta 0x06b2\n"
                   "lda 0x06bf\n"
                   "sta 0x06b3\n"
                   "mvi m, 0x0f\n"
                   "lxi sp, 0x06C0\n"
                   "mvi a, 0x0f\n"
                   "xra m\n"
                   "push psw\n"
                   "lda 0x06be\n"
                   "sta 0x06b4\n"
                   "lda 0x06bf\n"
                   "sta 0x06b5\n"
                   "hlt\n");
}

// LMEM: Memory dump 0x06B0 - 0x06B5 (6 bytes):
// LMEM: 06B0: 56 00 06 0F 46 00
// LMEM: "halt":"hlt"
