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
// RUN:   -d 0x0690:0x08 %t.bin 2>&1 | FileCheck %s --check-prefix=ACEDGE

__attribute__((naked)) int main(void) {
  __asm__ volatile("lxi sp, 0x06A0\n"
                   "mvi a, 0x0e\n"
                   "stc\n"
                   "aci 0x00\n"
                   "push psw\n"
                   "lda 0x069e\n"
                   "sta 0x0690\n"
                   "lda 0x069f\n"
                   "sta 0x0691\n"
                   "lxi sp, 0x06A0\n"
                   "mvi a, 0x0f\n"
                   "stc\n"
                   "aci 0x00\n"
                   "push psw\n"
                   "lda 0x069e\n"
                   "sta 0x0692\n"
                   "lda 0x069f\n"
                   "sta 0x0693\n"
                   "lxi sp, 0x06A0\n"
                   "mvi a, 0x10\n"
                   "stc\n"
                   "sbi 0x00\n"
                   "push psw\n"
                   "lda 0x069e\n"
                   "sta 0x0694\n"
                   "lda 0x069f\n"
                   "sta 0x0695\n"
                   "lxi sp, 0x06A0\n"
                   "mvi a, 0x01\n"
                   "stc\n"
                   "sbi 0x00\n"
                   "push psw\n"
                   "lda 0x069e\n"
                   "sta 0x0696\n"
                   "lda 0x069f\n"
                   "sta 0x0697\n"
                   "hlt\n");
}

// ACEDGE: Memory dump 0x0690 - 0x0697 (8 bytes):
// ACEDGE: 0690: 06 0F 12 10 16 0F 46 00
// ACEDGE: "halt":"hlt"
