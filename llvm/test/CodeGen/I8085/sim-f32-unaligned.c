// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %i8085_llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x20 %t.bin 2>&1 | FileCheck %s --check-prefix=F32UNALIGNED

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

TESTDUMP_U32(out, 8);

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

  union {
    uint32_t u;
    float f;
  } u5 = {0x007FFFFFu};
  volatile PackedF k = {0, u5.f};
  sum = k.f + j.f;
  out[5] = fbits(sum);

  union {
    uint32_t u;
    float f;
  } u6 = {0x80000001u};
  volatile PackedF l = {0, u6.f};
  sum = i.f + l.f;
  out[6] = fbits(sum);

  sum = j.f + l.f;
  out[7] = fbits(sum);

  return 0;
}

// F32UNALIGNED: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (32 bytes):
// F32UNALIGNED: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}01 00 00 00 01 00 00 00 01 00 00 00 03 00 00 00
// F32UNALIGNED: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}01 00 80 00 00 00 80 00 FF FF 7F 00 00 00 00 00
// F32UNALIGNED: "halt":"hlt"
