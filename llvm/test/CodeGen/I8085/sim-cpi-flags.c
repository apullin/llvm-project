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
// CPI: 05F8: 10 16 00 97
// CPI: "halt":"hlt"
