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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d 0x0200:0x02 %t.bin 2>&1 | FileCheck %s --check-prefix=FIB

#include <stdint.h>

int main(void) {
  volatile uint16_t *out = (uint16_t *)0x0200;
  uint16_t a = 0;
  uint16_t b = 1;
  for (uint8_t i = 0; i < 10; ++i) {
    uint16_t c = (uint16_t)(a + b);
    a = b;
    b = c;
  }
  *out = b; // fib(10) = 89 (0x0059)
  return 0;
}

// FIB: Memory dump 0x0200 - 0x0201 (2 bytes):
// FIB: 0200: 59 00
// FIB: "halt":"hlt"
