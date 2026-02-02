// REQUIRES: i8085-sim
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %S/../../../../../sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-sim-flat.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.elf %t.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d %testdump_addr:0x8 %t.bin 2>&1 | FileCheck %s --check-prefix=Q78

#include <stdint.h>

#include "sim-testdump.h"

TESTDUMP_I16(out, 4);

static int16_t q7_8_mul(int16_t a, int16_t b) {
  int32_t res = 0;
  int32_t aa = a;
  int32_t bb = b;
  int32_t sign = 1;

  if (aa < 0) {
    aa = -aa;
    sign = -sign;
  }
  if (bb < 0) {
    bb = -bb;
    sign = -sign;
  }

  while (bb) {
    if (bb & 1) {
      res += aa;
    }
    aa <<= 1;
    bb >>= 1;
  }

  if (sign < 0) {
    res = -res;
  }
  return (int16_t)(res >> 8);
}

int main(void) {
  const int16_t A[4] = {
      0x0100, 0x0080, // 1.0, 0.5
      0xFF00, 0x0200  // -1.0, 2.0
  };
  const int16_t B[4] = {
      0x0100, 0xFF80, // 1.0, -0.5
      0x0040, 0x0100  // 0.25, 1.0
  };

  out[0] = q7_8_mul(A[0], B[0]) + q7_8_mul(A[1], B[2]); // 0x0120
  out[1] = q7_8_mul(A[0], B[1]) + q7_8_mul(A[1], B[3]); // 0x0000
  out[2] = q7_8_mul(A[2], B[0]) + q7_8_mul(A[3], B[2]); // 0xFF80
  out[3] = q7_8_mul(A[2], B[1]) + q7_8_mul(A[3], B[3]); // 0x0280

  __asm__ volatile("hlt");
  return 0;
}

// Q78: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (8 bytes):
// Q78: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}20 01 00 00 80 FF 80 02
// Q78: "halt":"hlt"
