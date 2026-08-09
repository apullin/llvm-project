// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.elf %t.crt0.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 500000 -d %testdump_addr:0x04 %t.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=VLA
// RUN: %i8085_clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.o2.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.o2.elf %t.crt0.o %t.o2.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.o2.elf %t.o2.bin
// RUN: %i8085_sim -S -q -n 500000 -d %testdump_addr:0x04 %t.o2.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=VLA
// RUN: %i8085_clang -target i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.os.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.os.elf %t.crt0.o %t.os.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.os.elf %t.os.bin
// RUN: %i8085_sim -S -q -n 500000 -d %testdump_addr:0x04 %t.os.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=VLA

#include <stdint.h>

#include "sim-testdump.h"

TESTDUMP_U16(out, 2);

static volatile uint16_t element_count = 4;
static volatile uint16_t *escaped;

// Keep the callee opaque to the optimizer and force both halves of the i32
// return value into the ABI's BC:DE result registers.
__asm__(".text\n"
        ".globl clobber_de\n"
        "clobber_de:\n"
        "  lxi b, 0x1234\n"
        "  lxi d, 0xA500\n"
        "  ret\n");
extern uint32_t clobber_de(void);

__attribute__((noinline)) static uint32_t exercise_vla(void) {
  uint16_t values[element_count];
  escaped = values;
  values[0] = 0x5678;

  uint32_t result = clobber_de();
  return result + values[0];
}

int main(void) {
  uint32_t result = exercise_vla();
  out[0] = (uint16_t)result;
  out[1] = (uint16_t)(result >> 16);
  return 0;
}

// VLA: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (4 bytes):
// VLA: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}AC 68 00 A5
// VLA: "halt":"hlt"
