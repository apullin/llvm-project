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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d 0x0210:0x09 %t.bin 2>&1 | FileCheck %s --check-prefix=MEM

#include <stdint.h>

static const uint8_t src[8] = {1, 2, 3, 4, 5, 6, 7, 8};

int main(void) {
  volatile uint8_t *dst = (uint8_t *)0x0210;
  volatile uint8_t *sum_out = (uint8_t *)0x0218;
  uint8_t sum = 0;

  for (uint8_t i = 0; i < 8; ++i) {
    dst[i] = src[i];
    sum = (uint8_t)(sum + dst[i]);
  }

  *sum_out = sum; // 36 (0x24)
  return 0;
}

// MEM: Memory dump 0x0210 - 0x0218 (9 bytes):
// MEM: 0210: 01 02 03 04 05 06 07 08 24
// MEM: "halt":"hlt"
