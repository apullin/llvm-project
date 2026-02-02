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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 2000000 -d %testdump_addr:0x2 %t.bin 2>&1 | FileCheck %s --check-prefix=DEEPREC

#include <stdint.h>

#include "sim-testdump.h"

TESTDUMP_U8(out, 2);

__attribute__((noinline)) static uint8_t sum_down(uint8_t n);
typedef uint8_t (*sum_fn_t)(uint8_t);
static sum_fn_t volatile sum_down_ptr = sum_down;

__attribute__((noinline)) static uint8_t sum_down(uint8_t n) {
  if (n == 0) {
    return 0;
  }
  return (uint8_t)(n + sum_down_ptr((uint8_t)(n - 1u)));
}

int main(void) {
  const uint8_t depth = 64u;
  const uint8_t res = sum_down(depth);
  out[0] = res; // sum(1..64) = 2080 -> 0x20
  out[1] = 0;

  __asm__ volatile("hlt");
  return 0;
}

// DEEPREC: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (2 bytes):
// DEEPREC: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}20 00
// DEEPREC: "halt":"hlt"
