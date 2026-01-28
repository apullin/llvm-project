// REQUIRES: i8085-sim
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %S/../../../../../sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-sim-flat.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o
// RUN: llvm-objcopy -O binary %t.elf %t.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d 0x0202:0x02 %t.bin 2>&1 | FileCheck %s --check-prefix=SUM

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
