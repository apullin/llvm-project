// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %i8085_llc -O2 -verify-machineinstrs -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.elf %t.crt0.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 300000 -d %testdump_addr:0x8 %t.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=EXT
// RUN: %i8085_clang -target i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.os.bc
// RUN: %i8085_llc -O2 -verify-machineinstrs -mtriple=i8085-unknown-elf -filetype=obj %t.os.bc -o %t.os.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.os.elf %t.crt0.o %t.os.o
// RUN: %i8085_objcopy -O binary %t.os.elf %t.os.bin
// RUN: %i8085_sim -S -q -n 300000 -d %testdump_addr:0x8 %t.os.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=EXT

#include <stdint.h>

#include "sim-testdump.h"

TESTDUMP_I16(out, 4);

int a, b, *c = &b, e, f = 6, g, h;
short d;

static unsigned char foo(unsigned long long p1, int *p2) {
  for (; g <= 0; g++) {
    short *i = &d;
    int *j = &e;
    h = *c;
    *i = h;
    *j = (*i == *p2) < p1;
  }
  return 0;
}

int main(void) {
  foo(f, &a);
  out[0] = e == 1 ? e : (int16_t)0xdead;
  out[1] = g;
  out[2] = h;
  out[3] = d;
  return 0;
}

// EXT: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (8 bytes):
// EXT: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}01 00 01 00 00 00 00 00
// EXT: "halt":"hlt"
