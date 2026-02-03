// REQUIRES: i8085-sim
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %S/../../../../../sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.elf %t.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d 0x0202:0x4 %t.bin 2>&1 | FileCheck %s --check-prefix=SPILL

#include <stdint.h>

__attribute__((noinline)) static uint32_t spill_mix(uint8_t a, uint16_t b,
                                                    uint32_t c, uint8_t d) {
  volatile uint16_t locals[12];
  uint16_t base = (uint16_t)(a + b + d);
  for (uint8_t i = 0; i < 12; ++i) {
    locals[i] = (uint16_t)(base + i);
  }
  uint32_t r = c;
  for (uint8_t i = 0; i < 12; ++i) {
    r += locals[i];
  }
  return r;
}

int main(void) {
  volatile uint32_t *out = (uint32_t *)0x0202;
  *out = spill_mix(1, 0x2345, 0x12345678u, 2);
  return 0;
}

// SPILL: Memory dump 0x0202 - 0x0205 (4 bytes):
// SPILL: 0202: 1A FE 35 12
// SPILL: "halt":"hlt"
