// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %i8085_llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 200000 -d 0x0200:0x4 %t.bin 2>&1 | FileCheck %s --check-prefix=ABI

#include <stdint.h>

static volatile uint64_t in_val = 0x0123456789ABCDEFULL;

__attribute__((noinline)) static uint32_t conv(uint64_t x) {
  return (uint32_t)x;
}

int main(void) {
  volatile uint8_t *p = (volatile uint8_t *)0x0200;
  volatile uint32_t v = conv(in_val);
  uint32_t u = (uint32_t)v;
  p[0] = (uint8_t)(u >> 0);
  p[1] = (uint8_t)(u >> 8);
  p[2] = (uint8_t)(u >> 16);
  p[3] = (uint8_t)(u >> 24);
  return 0;
}

// ABI: Memory dump 0x0200 - 0x0203 (4 bytes):
// ABI: 0200: EF CD AB 89
// ABI: "halt":"hlt"
