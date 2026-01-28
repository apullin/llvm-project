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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d 0x0202:0x04 %t.bin 2>&1 | FileCheck %s --check-prefix=CALLS

#include <stdint.h>

static uint32_t mix(uint8_t a, uint16_t b, uint32_t c, uint8_t d,
                    uint16_t e, uint32_t f, uint8_t g, uint16_t h) {
  uint32_t r = c + f;
  r += (uint32_t)a + d + g;
  r += (uint32_t)b + e + h;
  r ^= 0x00FF00FFu;
  return r;
}

int main(void) {
  volatile uint32_t *out = (uint32_t *)0x0202;
  uint32_t v = mix(1, 0x0203, 0x11223344u, 2, 0x0304, 0x55667788u, 3, 0x0405);
  *out = v; // expect 0x6677B321
  return 0;
}

// CALLS: Memory dump 0x0202 - 0x0205 (4 bytes):
// CALLS: 0202: 21 B3 77 66
// CALLS: "halt":"hlt"
