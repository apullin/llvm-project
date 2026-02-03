// REQUIRES: i8085-sim
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %S/../../../../../sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.elf %t.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d 0x0200:0x2 %t.bin 2>&1 | FileCheck %s --check-prefix=ABI

#include <stdint.h>

static volatile int16_t in_val = -0x1234;

__attribute__((noinline)) static uint16_t conv(int16_t x) {
  return (uint16_t)x;
}

int main(void) {
  volatile uint8_t *p = (volatile uint8_t *)0x0200;
  volatile uint16_t v = conv(in_val);
  uint16_t u = (uint16_t)v;
  p[0] = (uint8_t)(u >> 0);
  p[1] = (uint8_t)(u >> 8);
  return 0;
}

// ABI: Memory dump 0x0200 - 0x0201 (2 bytes):
// ABI: 0200: CC ED
// ABI: "halt":"hlt"
