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
// RUN: %i8085_sim -S -q -n 200000 -d 0x0202:0x02 %t.bin 2>&1 | FileCheck %s --check-prefix=SUM

#include <stdint.h>

static const uint8_t data[5] = {1, 2, 3, 4, 5};

int main(void) {
  volatile uint16_t *out = (uint16_t *)0x0202;
  uint16_t sum = 0;
  for (uint8_t i = 0; i < 5; ++i) {
    sum = (uint16_t)(sum + data[i]);
  }
  *out = sum; // 1+2+3+4+5 = 15 (0x000F)
  return 0;
}

// SUM: Memory dump 0x0202 - 0x0203 (2 bytes):
// SUM: 0202: 0F 00
// SUM: "halt":"hlt"
