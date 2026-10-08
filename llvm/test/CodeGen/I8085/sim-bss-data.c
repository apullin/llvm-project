// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %i8085_llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 200000 -d 0x0200:0x02 %t.bin 2>&1 | FileCheck %s --check-prefix=BSS

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
