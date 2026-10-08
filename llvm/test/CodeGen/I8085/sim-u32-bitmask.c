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
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x8 %t.bin 2>&1 | FileCheck %s --check-prefix=U32MASK

#include <stdint.h>

#include "sim-testdump.h"

TESTDUMP_U32(out, 2);

int main(void) {
  uint32_t a = 0x3FC00000u;
  uint32_t b = 0xBFC00000u;

  out[0] = a & 0x7FFFFFFFu; // expect 0x3FC00000
  out[1] = b & 0x7FFFFFFFu; // expect 0x3FC00000
  return 0;
}

// U32MASK: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (8 bytes):
// U32MASK: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}00 00 C0 3F 00 00 C0 3F
// U32MASK: "halt":"hlt"
