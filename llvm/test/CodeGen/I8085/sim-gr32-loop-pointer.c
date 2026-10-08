// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.elf %t.crt0.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 500000 -d %testdump_addr:0x02 %t.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=GR32
// RUN: %i8085_clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.o2.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.o2.elf %t.crt0.o %t.o2.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.o2.elf %t.o2.bin
// RUN: %i8085_sim -S -q -n 500000 -d %testdump_addr:0x02 %t.o2.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=GR32
// RUN: %i8085_clang -target i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.os.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.os.elf %t.crt0.o %t.os.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.os.elf %t.os.bin
// RUN: %i8085_sim -S -q -n 500000 -d %testdump_addr:0x02 %t.os.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=GR32

#include <stdint.h>

#include "sim-testdump.h"

TESTDUMP_U16(out, 1);

struct chain {
  struct chain *next;
  uint16_t index;
};

uint32_t contents[2][2];

__attribute__((noinline)) static uint16_t check(const uint32_t values[2]) {
  return values[0] == UINT32_MAX && values[1] == UINT32_MAX ? 0x5a2e
                                                            : 0xdead;
}

__attribute__((noinline)) static uint16_t accumulate(struct chain *first) {
  uint32_t values[2] = {0, 0};
  struct chain *last;
  uint16_t uses = 0;

  for (last = first; last->next; last = last->next) {
    values[0] |= ~contents[last->index][0];
    values[1] |= ~contents[last->index][1];
    ++uses;
  }

  if (uses != 0) {
    values[0] |= ~contents[last->index][0];
    values[1] |= ~contents[last->index][1];
  }
  return check(values);
}

int main(void) {
  struct chain tail = {0, 0};
  struct chain head = {&tail, 1};

  contents[0][0] = UINT32_MAX;
  contents[0][1] = UINT32_MAX;
  contents[1][0] = 0;
  contents[1][1] = 0;
  out[0] = accumulate(&head);
  return 0;
}

// GR32: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (2 bytes):
// GR32: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}2E 5A
// GR32: "halt":"hlt"
