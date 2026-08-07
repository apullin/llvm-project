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
// RUN: %i8085_sim -S -q -n 200000 -d 0x0200:0x8 %t.bin 2>&1 | FileCheck %s --check-prefix=ABI

#include <stdint.h>

static volatile int16_t in_val = -0x1234;

__attribute__((noinline)) static int64_t conv(int16_t x) {
  return (int64_t)x;
}

int main(void) {
  volatile uint8_t *p = (volatile uint8_t *)0x0200;
  volatile int64_t v = conv(in_val);
  volatile uint8_t *vp = (volatile uint8_t *)&v;
  p[0] = vp[0];
  p[1] = vp[1];
  p[2] = vp[2];
  p[3] = vp[3];
  p[4] = vp[4];
  p[5] = vp[5];
  p[6] = vp[6];
  p[7] = vp[7];
  return 0;
}

// ABI: Memory dump 0x0200 - 0x0207 (8 bytes):
// ABI: 0200: CC ED FF FF FF FF FF FF
// ABI: "halt":"hlt"
