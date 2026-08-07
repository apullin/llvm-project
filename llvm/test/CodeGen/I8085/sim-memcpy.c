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
// RUN: %i8085_sim -S -q -n 200000 -d 0x0210:0x09 %t.bin 2>&1 | FileCheck %s --check-prefix=MEM
// RUN: %i8085_clang -target i8085-unknown-elf -O1 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.o1.bc
// RUN: %i8085_llc -O1 -mtriple=i8085-unknown-elf -filetype=obj %t.o1.bc -o %t.o1.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.o1.map \
// RUN:   -o %t.o1.elf %t.crt0.o %t.o1.o
// RUN: %i8085_objcopy -O binary %t.o1.elf %t.o1.bin
// RUN: %i8085_sim -S -q -n 200000 -d 0x0210:0x09 %t.o1.bin 2>&1 | FileCheck %s --check-prefix=MEM
// RUN: %i8085_clang -target i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.os.bc
// RUN: %i8085_llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.os.bc -o %t.os.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.os.map \
// RUN:   -o %t.os.elf %t.crt0.o %t.os.o
// RUN: %i8085_objcopy -O binary %t.os.elf %t.os.bin
// RUN: %i8085_sim -S -q -n 200000 -d 0x0210:0x09 %t.os.bin 2>&1 | FileCheck %s --check-prefix=MEM
// RUN: %i8085_clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.o2.bc
// RUN: %i8085_llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.o2.bc -o %t.o2.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.o2.map \
// RUN:   -o %t.o2.elf %t.crt0.o %t.o2.o
// RUN: %i8085_objcopy -O binary %t.o2.elf %t.o2.bin
// RUN: %i8085_sim -S -q -n 200000 -d 0x0210:0x09 %t.o2.bin 2>&1 | FileCheck %s --check-prefix=MEM

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
