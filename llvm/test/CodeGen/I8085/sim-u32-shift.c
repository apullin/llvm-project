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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 2000000 -d %testdump_addr:0x24 %t.bin 2>&1 | FileCheck %s --check-prefix=SH32

#include <stdint.h>

#include "sim-testdump.h"

TESTDUMP_U32(out, 9);

int main(void) {
  uint32_t v = 0x12345678u;
  uint32_t v2 = 1u;
  int32_t sv = (int32_t)0x80000000u;

  out[0] = v << 1;   // 0x2468ACF0
  out[1] = v << 8;   // 0x34567800
  out[2] = v << 16;  // 0x56780000
  out[3] = v >> 1;   // 0x091A2B3C
  out[4] = v >> 8;   // 0x00123456
  out[5] = v >> 16;  // 0x00001234
  out[6] = v2 << 31; // 0x80000000
  out[7] = v2 >> 31; // 0x00000000
  out[8] = (uint32_t)(sv >> 1); // 0xC0000000
  return 0;
}

// SH32: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (36 bytes):
// SH32: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}F0 AC 68 24 00 78 56 34 00 00 78 56 3C 2B 1A 09
// SH32: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}56 34 12 00 34 12 00 00 00 00 00 80 00 00 00 00
// SH32: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}00 00 00 C0
// SH32: "halt":"hlt"
