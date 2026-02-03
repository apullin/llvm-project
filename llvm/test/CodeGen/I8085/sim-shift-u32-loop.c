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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 2000000 -d %testdump_addr:0x8 %t.bin 2>&1 | FileCheck %s --check-prefix=SHU32

#include <stdint.h>

#include "sim-testdump.h"

TESTDUMP_U32(out, 2);

int main(void) {
  uint32_t v = 0x81234567;
  uint32_t acc_l = 0;
  uint32_t acc_r = 0;
  for (uint8_t s = 0; s < 32; ++s) {
    acc_l ^= (uint32_t)(v << s);
    acc_r ^= (uint32_t)(v >> s);
  }
  out[0] = acc_l;
  out[1] = acc_r;
  return 0;
}

// SHU32: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (8 bytes):
// SHU32: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}DD 3C E1 80 45 86 3D FE
// SHU32: "halt":"hlt"
