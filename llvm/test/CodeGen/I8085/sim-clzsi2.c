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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d %testdump_addr:0x10 %t.bin 2>&1 | FileCheck %s --check-prefix=CLZ

#include <stdint.h>

#include "sim-testdump.h"

extern int32_t __clzsi2(int32_t);

TESTDUMP_U32(out, 4);

int main(void) {
  out[0] = (uint32_t)__clzsi2((int32_t)0x00400000L); // expect 9
  out[1] = (uint32_t)__clzsi2((int32_t)0x00000001L); // expect 31
  out[2] = (uint32_t)__clzsi2((int32_t)0x80000000u); // expect 0
  out[3] = (uint32_t)__clzsi2((int32_t)0x00000000L); // expect 32 (impl-defined but ours returns 32)
  return 0;
}

// CLZ: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (16 bytes):
// CLZ: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}09 00 00 00 1F 00 00 00 00 00 00 00 20 00 00 00
// CLZ: "halt":"hlt"
