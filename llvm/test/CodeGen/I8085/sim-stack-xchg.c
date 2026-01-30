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
// RUN:   -d 0x03A0:0x0A %t.bin 2>&1 | FileCheck %s --check-prefix=STK

__attribute__((naked)) int main(void) {
  __asm__ volatile("lxi sp, 0x0400\n"
                   "lxi h, 0x1234\n"
                   "mvi a, 0xAA\n"
                   "sta 0x0400\n"
                   "mvi a, 0xBB\n"
                   "sta 0x0401\n"
                   "xthl\n"
                   "shld 0x03A0\n"
                   "lda 0x0400\n"
                   "sta 0x03A2\n"
                   "lda 0x0401\n"
                   "sta 0x03A3\n"
                   "lxi d, 0x2222\n"
                   "lxi h, 0x1111\n"
                   "xchg\n"
                   "shld 0x03A4\n"
                   "mov a, d\n"
                   "sta 0x03A6\n"
                   "mov a, e\n"
                   "sta 0x03A7\n"
                   "mvi a, 0x5A\n"
                   "stc\n"
                   "push psw\n"
                   "mvi a, 0x00\n"
                   "cmc\n"
                   "pop psw\n"
                   "sta 0x03A8\n"
                   "mvi a, 0x00\n"
                   "adc a\n"
                   "sta 0x03A9\n"
                   "hlt\n");
}

// STK: Memory dump 0x03A0 - 0x03A9 (10 bytes):
// STK: 03A0: AA BB 34 12 22 22 11 11 5A 01
// STK: "halt":"hlt"
