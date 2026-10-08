// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.elf %t.crt0.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x18 %t.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=EXT
// RUN: %i8085_clang -target i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.os.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.os.elf %t.crt0.o %t.os.o
// RUN: %i8085_objcopy -O binary %t.os.elf %t.os.bin
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x18 %t.os.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=EXT

#include <stdint.h>

#include "sim-testdump.h"

TESTDUMP_I32(out, 6);

static uint16_t p;

__attribute__((noinline)) static int32_t test(uint16_t a) {
  return (int32_t)(uint16_t)(p + a) - (int32_t)p;
}

int main(void) {
  p = (uint16_t)-2;
  out[0] = test(0);
  out[1] = test(1);
  out[2] = test(2);
  p = (uint16_t)-1;
  out[3] = test(0);
  out[4] = test(1);
  out[5] = test(2);
  return 0;
}

// EXT: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (24 bytes):
// EXT: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}00 00 00 00 01 00 00 00 02 00 FF FF 00 00 00 00
// EXT: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}01 00 FF FF 02 00 FF FF
// EXT: "halt":"hlt"
