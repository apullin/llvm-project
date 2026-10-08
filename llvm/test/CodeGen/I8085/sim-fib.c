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
// RUN: %i8085_sim -S -q -n 200000 -d 0x0200:0x02 %t.bin 2>&1 | FileCheck %s --check-prefix=FIB
// RUN: %i8085_clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.o2.bc
// RUN: %i8085_llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.o2.bc -o %t.o2.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.o2.map \
// RUN:   -o %t.o2.elf %t.crt0.o %t.o2.o
// RUN: %i8085_objcopy -O binary %t.o2.elf %t.o2.bin
// RUN: %i8085_sim -S -q -n 200000 -d 0x0200:0x02 %t.o2.bin 2>&1 | FileCheck %s --check-prefix=FIB

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
