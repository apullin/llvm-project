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
// RUN:   -d 0x05F8:0x04 %t.bin 2>&1 | FileCheck %s --check-prefix=CPI

__attribute__((naked)) int main(void) {
  __asm__ volatile("lxi sp, 0x0600\n"
                   "mvi a, 0x10\n"
                   "cpi 0x01\n"
                   "push psw\n"
                   "lda 0x05ff\n"
                   "sta 0x05f8\n"
                   "lda 0x05fe\n"
                   "sta 0x05f9\n"
                   "lxi sp, 0x0600\n"
                   "mvi a, 0x00\n"
                   "cpi 0x01\n"
                   "push psw\n"
                   "lda 0x05ff\n"
                   "sta 0x05fa\n"
                   "lda 0x05fe\n"
                   "sta 0x05fb\n"
                   "hlt\n");
}

// CPI: Memory dump 0x05F8 - 0x05FB (4 bytes):
// CPI: 05F8: 10 14 00 95
// CPI: "halt":"hlt"
