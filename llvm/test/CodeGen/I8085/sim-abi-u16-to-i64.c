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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d 0x0200:0x8 %t.bin 2>&1 | FileCheck %s --check-prefix=ABI

#include <stdint.h>

static volatile uint16_t in_val = 0xA5A5u;

__attribute__((noinline)) static int64_t conv(uint16_t x) {
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
// ABI: 0200: A5 A5 00 00 00 00 00 00
// ABI: "halt":"hlt"
