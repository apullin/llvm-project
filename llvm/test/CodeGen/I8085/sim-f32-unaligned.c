// REQUIRES: i8085-sim
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %S/../../../../../sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-sim-flat.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.elf %t.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d %testdump_addr:0x14 %t.bin 2>&1 | FileCheck %s --check-prefix=F32UNALIGNED

#include <stdint.h>

#include "sim-testdump.h"

typedef struct __attribute__((packed)) {
  uint8_t pad;
  float f;
} PackedF;

static uint32_t fbits(float f) {
  union {
    float f;
    uint32_t u;
  } x;
  x.f = f;
  return x.u;
}

TESTDUMP_U32(out, 5);

int main(void) {
  volatile PackedF a = {0, 1.0f};
  volatile PackedF b = {0, 1e-30f};
  float sum = a.f + b.f;
  out[0] = (fbits(sum) == fbits(a.f));

  volatile PackedF c = {0, -1.0f};
  volatile PackedF d = {0, -1e-30f};
  sum = c.f + d.f;
  out[1] = (fbits(sum) == fbits(c.f));

  volatile PackedF e = {0, 1e10f};
  volatile PackedF f = {0, 1.0f};
  sum = e.f + f.f;
  out[2] = (fbits(sum) == fbits(e.f));

  union {
    uint32_t u;
    float f;
  } u1 = {0x00000001u};
  union {
    uint32_t u;
    float f;
  } u2 = {0x00000002u};
  volatile PackedF g = {0, u1.f};
  volatile PackedF h = {0, u2.f};
  sum = g.f + h.f;
  out[3] = fbits(sum);

  union {
    uint32_t u;
    float f;
  } u3 = {0x00800000u};
  union {
    uint32_t u;
    float f;
  } u4 = {0x00000001u};
  volatile PackedF i = {0, u3.f};
  volatile PackedF j = {0, u4.f};
  sum = i.f + j.f;
  out[4] = fbits(sum);

  return 0;
}

// F32UNALIGNED: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (20 bytes):
// F32UNALIGNED: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}01 00 00 00 01 00 00 00 01 00 00 00 03 00 00 00
// F32UNALIGNED: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}01 00 80 00
// F32UNALIGNED: "halt":"hlt"
