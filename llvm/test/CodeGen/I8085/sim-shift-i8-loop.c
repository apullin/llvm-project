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
// RUN: %i8085_sim -S -q -n 2000000 -d %testdump_addr:0x2 %t.bin 2>&1 | FileCheck %s --check-prefix=SHI8

#include <stdint.h>

#include "sim-testdump.h"

TESTDUMP_U8(out, 2);

int main(void) {
  int8_t pos = 1;
  int8_t neg = (int8_t)0x81;
  uint8_t acc_l = 0;
  uint8_t acc_r = 0;
  for (uint8_t s = 0; s < 8; ++s) {
    acc_l ^= (uint8_t)(pos << s);
    acc_r ^= (uint8_t)(neg >> s);
  }
  out[0] = acc_l;
  out[1] = acc_r;
  return 0;
}

// SHI8: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (2 bytes):
// SHI8: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}FF 54
// SHI8: "halt":"hlt"
