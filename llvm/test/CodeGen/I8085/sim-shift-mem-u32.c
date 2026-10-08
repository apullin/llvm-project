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
// RUN: %i8085_sim -S -q -n 2000000 -d %testdump_addr:0x10 %t.bin 2>&1 | FileCheck %s --check-prefix=SHM32

#include <stdint.h>

#include "sim-testdump.h"

static uint32_t g = 0x89ABCDEFu;
static volatile uint32_t gv = 0x80000001u;
static volatile uint8_t gs = 7;

__attribute__((noinline)) static uint32_t shift_from_ptr(const uint32_t *p,
                                                        uint8_t s) {
  uint32_t v = *p;
  return v << s;
}

TESTDUMP_U32(out, 4);

int main(void) {
  uint32_t v = 0x12345678u;

  out[0] = shift_from_ptr(&v, 5);
  out[1] = g >> 3;
  out[2] = gv >> 4;
  out[3] = g << gs;
  return 0;
}

// SHM32: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (16 bytes):
// SHM32: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}00 CF 8A 46 BD 79 35 11 00 00 00 08 80 F7 E6 D5
// SHM32: "halt":"hlt"
