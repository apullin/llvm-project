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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d 0x0206:0x06 %t.bin 2>&1 | FileCheck %s --check-prefix=CMP

#include <stdint.h>

static const int16_t vals[6] = {-3, -1, 0, 1, 2, 7};

int main(void) {
  volatile uint16_t *out = (uint16_t *)0x0206;
  uint16_t neg = 0;
  uint16_t le1 = 0;
  uint16_t bigu = 0;

  for (uint8_t i = 0; i < 6; ++i) {
    int16_t v = vals[i];
    if (v < 0)
      neg++;
    if (v <= 1)
      le1++;
    if ((uint16_t)v >= 0xFF00u)
      bigu++;
  }

  out[0] = neg;  // expect 2
  out[1] = le1;  // expect 4
  out[2] = bigu; // expect 2
  return 0;
}

// CMP: Memory dump 0x0206 - 0x020B (6 bytes):
// CMP: 0206: 02 00 04 00 02 00
// CMP: "halt":"hlt"
