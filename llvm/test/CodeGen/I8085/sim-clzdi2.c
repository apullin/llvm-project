// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x14 %t.bin 2>&1 | FileCheck %s --check-prefix=CLZDI2

#include <stdint.h>

#include "sim-testdump.h"

TESTDUMP_U32(out, 5);

static volatile uint64_t in[5] = {
    1ULL,
    0x0000000100000000ULL,
    123456ULL,
    0xF0F0F0ULL,
    0x8000000000000000ULL,
};

int main(void) {
  out[0] = __builtin_clzll(in[0]);
  out[1] = __builtin_clzll(in[1]);
  out[2] = __builtin_clzll(in[2]);
  out[3] = __builtin_clzll(in[3]);
  out[4] = __builtin_clzll(in[4]);
  return 0;
}

// CLZDI2: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (20 bytes):
// CLZDI2: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}3F 00 00 00 1F 00 00 00 2F 00 00 00 28 00 00 00
// CLZDI2: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}00 00 00 00
// CLZDI2: "halt":"hlt"
