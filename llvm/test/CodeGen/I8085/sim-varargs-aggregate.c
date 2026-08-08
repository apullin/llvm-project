// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.elf %t.crt0.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 300000 -d %testdump_addr:0x12 %t.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=VARARG
// RUN: %i8085_clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.o2.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.o2.elf %t.crt0.o %t.o2.o
// RUN: %i8085_objcopy -O binary %t.o2.elf %t.o2.bin
// RUN: %i8085_sim -S -q -n 300000 -d %testdump_addr:0x12 %t.o2.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=VARARG
// RUN: %i8085_clang -target i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.os.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.os.elf %t.crt0.o %t.os.o
// RUN: %i8085_objcopy -O binary %t.os.elf %t.os.bin
// RUN: %i8085_sim -S -q -n 300000 -d %testdump_addr:0x12 %t.os.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=VARARG

#include <stdarg.h>
#include <stdint.h>

#include "sim-testdump.h"

struct One {
  uint16_t x;
};

struct Five {
  uint8_t a, b, c, d, e;
};

struct Pair {
  int16_t x, y;
};

TESTDUMP_U16(out, 9);

__attribute__((noinline)) static void capture(int ignored, ...) {
  va_list ap;
  va_start(ap, ignored);
  struct One first = va_arg(ap, struct One);
  struct One second = va_arg(ap, struct One);
  struct Five five = va_arg(ap, struct Five);
  int marker = va_arg(ap, int);
  struct Pair pair = va_arg(ap, struct Pair);
  va_end(ap);

  out[0] = first.x;
  out[1] = second.x;
  out[2] = (uint16_t)five.a | ((uint16_t)five.b << 8);
  out[3] = (uint16_t)five.c | ((uint16_t)five.d << 8);
  out[4] = five.e;
  out[5] = (uint16_t)marker;
  out[6] = (uint16_t)pair.x;
  out[7] = (uint16_t)pair.y;
}

__attribute__((noinline)) static struct One pick(int ignored, ...) {
  va_list ap;
  va_start(ap, ignored);
  (void)va_arg(ap, struct One);
  struct One result = va_arg(ap, struct One);
  va_end(ap);
  return result;
}

int main(void) {
  struct One first = {10};
  struct One second = {20};
  struct Five five = {11, 22, 33, 44, 55};
  struct Pair pair = {-1, 0x1111};

  capture(5, first, second, five, 0x1234, pair);
  out[8] = pick(2, first, second).x;
  return 0;
}

// VARARG: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (18 bytes):
// VARARG: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}0A 00 14 00 0B 16 21 2C 37 00 34 12 FF FF 11 11
// VARARG: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}14 00
// VARARG: "halt":"hlt"
