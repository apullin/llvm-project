// REQUIRES: i8085-sim
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %S/../../../../../sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o
// RUN: llvm-objcopy -O binary %t.elf %t.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d 0x0200:0x02 %t.bin 2>&1 | FileCheck %s --check-prefix=BSS

#include <stdint.h>

static uint16_t bss_word;
static uint8_t bss_bytes[3];

static uint16_t data_word = 0x1234;
static uint8_t data_bytes[4] = {0x11, 0x22, 0x33, 0x44};

int main(void) {
  volatile uint16_t *out = (uint16_t *)0x0200;
  uint16_t ok = 0;

  if (bss_word == 0)
    ok++;
  if (bss_bytes[2] == 0)
    ok++;
  if (data_word == 0x1234)
    ok++;
  if (data_bytes[1] == 0x22)
    ok++;

  *out = ok; // expect 4
  return 0;
}

// BSS: Memory dump 0x0200 - 0x0201 (2 bytes):
// BSS: 0200: 04 00
// BSS: "halt":"hlt"
