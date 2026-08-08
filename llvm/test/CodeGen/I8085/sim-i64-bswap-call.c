// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %i8085_llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-32kram-32krom.ld \
// RUN:   -o %t.elf %t.crt0.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 200000 -d 0x0600:0x10 %t.bin 2>&1 \
// RUN:   | FileCheck %s --check-prefix=RESULT
// RUN: %i8085_clang -target i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.os.bc
// RUN: %i8085_llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.os.bc -o %t.os.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-32kram-32krom.ld \
// RUN:   -o %t.os.elf %t.crt0.o %t.os.o
// RUN: %i8085_objcopy -O binary %t.os.elf %t.os.bin
// RUN: %i8085_sim -S -q -n 200000 -d 0x0600:0x10 %t.os.bin 2>&1 \
// RUN:   | FileCheck %s --check-prefix=RESULT

#include <stdint.h>

volatile uint64_t input = UINT64_C(0x123456789abcdef0);

__attribute__((noinline)) uint64_t swap64(uint64_t value) {
  return __builtin_bswap64(value);
}

int main(void) {
  uint64_t value = input;
  uint64_t swapped = swap64(value);
  volatile uint8_t *out = (volatile uint8_t *)0x0600;
  const uint8_t *value_bytes = (const uint8_t *)&value;
  const uint8_t *swapped_bytes = (const uint8_t *)&swapped;

  for (unsigned i = 0; i != 8; ++i) {
    out[i] = value_bytes[i];
    out[i + 8] = swapped_bytes[i];
  }
  return 0;
}

// RESULT: Memory dump 0x0600 - 0x060F (16 bytes):
// RESULT: 0600: F0 DE BC 9A 78 56 34 12 12 34 56 78 9A BC DE F0
// RESULT: "halt":"hlt"
