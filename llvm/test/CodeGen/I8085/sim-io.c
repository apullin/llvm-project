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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 --io=0x10:0x3C -d 0x0230:0x02 %t.bin 2>&1 | FileCheck %s --check-prefix=IO

int main(void) {
  __asm__ volatile("in 0x10\n"
                   "sta 0x0230\n"
                   "mvi a, 0xA5\n"
                   "out 0x11\n"
                   "in 0x11\n"
                   "sta 0x0231\n");
  return 0;
}

// IO: Memory dump 0x0230 - 0x0231 (2 bytes):
// IO: 0230: 3C A5
// IO: "halt":"hlt"
