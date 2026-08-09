// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.elf %t.crt0.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 500000 -d %testdump_addr:0x02 %t.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=VLA
// RUN: %i8085_clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.o2.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.o2.elf %t.crt0.o %t.o2.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.o2.elf %t.o2.bin
// RUN: %i8085_sim -S -q -n 500000 -d %testdump_addr:0x02 %t.o2.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=VLA
// RUN: %i8085_clang -target i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.os.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
// RUN:   -o %t.os.elf %t.crt0.o %t.os.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.os.elf %t.os.bin
// RUN: %i8085_sim -S -q -n 500000 -d %testdump_addr:0x02 %t.os.bin 2>&1 | \
// RUN:   FileCheck %s --check-prefix=VLA

#include <stdint.h>

#include "sim-testdump.h"

TESTDUMP_U16(out, 1);

static volatile uint16_t count = 9;
static volatile char *escaped;
static const char input[] = "test1234/";
static const char suffix[] = "/./";

// Copy the first byte of the second stack argument through the first, then
// deliberately clobber DE. This simultaneously checks outgoing pointer
// arguments and the caller's fixed-frame base preservation.
__asm__(".text\n"
        ".globl copy_one_and_clobber_de\n"
        "copy_one_and_clobber_de:\n"
        "  lxi h, 2\n"
        "  dad sp\n"
        "  mov e, m\n"
        "  inx h\n"
        "  mov d, m\n"
        "  inx h\n"
        "  mov c, m\n"
        "  inx h\n"
        "  mov b, m\n"
        "  ldax b\n"
        "  stax d\n"
        "  lxi d, 0xA55A\n"
        "  ret\n");
extern void copy_one_and_clobber_de(char *dst, const char *src);

__attribute__((noinline)) static uint16_t exercise(void) {
  char buf[count + 1];
  escaped = buf;
  buf[0] = 0x5a;
  buf[count] = 0;

  copy_one_and_clobber_de(&buf[count],
                          &suffix[input[count - 1] == '/']);
  return ((uint16_t)(uint8_t)buf[0] << 8) | (uint8_t)buf[count];
}

int main(void) {
  out[0] = exercise();
  return 0;
}

// VLA: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (2 bytes):
// VLA: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}2E 5A
// VLA: "halt":"hlt"
