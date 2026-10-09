// REQUIRES: i8085-sim
// RUN: %i8085_clang --target=i8085-unknown-elf -c %i8085_sdk/sysroot/crt/crt0.S -o %t.crt.o
// RUN: %i8085_clang --target=i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -c %s -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x10 %t.bin 2>&1 | FileCheck %s
// RUN: %i8085_clang --target=i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -c %s -o %t.os.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.os.elf %t.crt.o %t.os.o
// RUN: %i8085_objcopy -O binary %t.os.elf %t.os.bin
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x10 %t.os.bin 2>&1 | FileCheck %s
// RUN: %i8085_clang --target=i8085-unknown-elf -Oz -ffreestanding -fno-builtin -nostdlib -c %s -o %t.oz.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.oz.elf %t.crt.o %t.oz.o
// RUN: %i8085_objcopy -O binary %t.oz.elf %t.oz.bin
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x10 %t.oz.bin 2>&1 | FileCheck %s

#include <stdint.h>
#include "sim-testdump.h"

TESTDUMP_U16(out, 8);

__attribute__((noinline)) uint16_t swap_sub(uint16_t *p, uint16_t *q) {
  uint16_t a = *p, b = *q;
  if (a != b) {
    *p = b;
    *q = a;
  }
  return a - b;
}

__attribute__((noinline)) uint16_t sub_reuse(uint16_t a, uint16_t b) {
  uint16_t difference = a - b;
  out[3] = a;
  out[4] = b;
  return difference;
}

__attribute__((noinline)) uint16_t update(uint16_t *p, uint16_t delta) {
  uint16_t old = *p;
  *p = old + delta;
  return old;
}

int main(void) {
  uint16_t a = 0x1234, b = 0xff01;
  out[0] = swap_sub(&a, &b);
  out[1] = a;
  out[2] = b;
  out[5] = sub_reuse(a, b);
  out[6] = update(&b, 0xffff);
  out[7] = b;
  return 0;
}

// CHECK: Memory dump {{0x[0-9A-F]+}} - {{0x[0-9A-F]+}} (16 bytes):
// CHECK: {{ *}}{{[0-9A-F]+}}:{{ *}}33 13 01 FF 34 12 01 FF 34 12 CD EC 34 12 33 12
// CHECK: "halt":"hlt"
