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
// RUN: %i8085_sim -S -q -n 200000 --io=0x10:0x3C \
// RUN:   -d 0x0700:0x04 %t.bin 2>&1 | FileCheck %s --check-prefix=IOFLAGS

__attribute__((naked)) int main(void) {
  __asm__ volatile("lxi sp, 0x06A0\n"
                   "mvi a, 0x0f\n"
                   "adi 0x01\n"
                   "in 0x10\n"
                   "push psw\n"
                   "lda 0x069e\n"
                   "sta 0x0700\n"
                   "lda 0x069f\n"
                   "sta 0x0701\n"
                   "lxi sp, 0x06A0\n"
                   "out 0x11\n"
                   "push psw\n"
                   "lda 0x069e\n"
                   "sta 0x0702\n"
                   "lda 0x069f\n"
                   "sta 0x0703\n"
                   "hlt\n");
}

// IOFLAGS: Memory dump 0x0700 - 0x0703 (4 bytes):
// IOFLAGS: 0700: 10 3C 10 3C
// IOFLAGS: "halt":"hlt"
