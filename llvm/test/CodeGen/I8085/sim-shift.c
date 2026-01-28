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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d 0x0220:0x08 %t.bin 2>&1 | FileCheck %s --check-prefix=SHIFT

#include <stdint.h>

int main(void) {
  volatile uint8_t *out8 = (uint8_t *)0x0220;
  volatile uint8_t *pad = (uint8_t *)0x0221;
  volatile uint16_t *out16 = (uint16_t *)0x0222;
  volatile uint32_t *out32 = (uint32_t *)0x0224;

  uint8_t a = 0x81u;
  uint8_t b = (uint8_t)(a >> 3); // 0x10
  uint16_t c = 0x1234u;
  uint16_t d = (uint16_t)(c << 3); // 0x91A0
  uint32_t e = 0x80000001u;
  uint32_t f = e >> 4; // 0x08000000

  *out8 = b;
  *pad = 0x55;
  *out16 = d;
  *out32 = f;
  return 0;
}

// SHIFT: Memory dump 0x0220 - 0x0227 (8 bytes):
// SHIFT: 0220: 10 55 A0 91 00 00 00 08
// SHIFT: "halt":"hlt"
