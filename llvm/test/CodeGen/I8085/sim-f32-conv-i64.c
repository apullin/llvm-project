// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %i8085_llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat-hi.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 200000 -d 0xFF40:0x18 %t.bin 2>&1 | FileCheck %s --check-prefix=F32CONV64

#include <stdint.h>

#include "sim-testdump.h"

extern float __floatdisf(int64_t);
extern float __floatundisf(uint64_t);
extern int64_t __fixsfdi(float);
extern uint64_t __fixunssfdi(float);

struct TestDump {
  float outf[2];
  int64_t outs;
  uint64_t outu;
} __attribute__((packed));

volatile struct TestDump out TESTDUMP_SECTION TESTDUMP_ALIGN;

int main(void) {
  int64_t si = -123456;
  uint64_t ui = 0x0000000000F0F0F0ULL;
  float fpos = 123456.0f;
  float fneg = -123456.0f;

  out.outf[0] = __floatdisf(si);    // -123456.0f -> 0xC7F12000
  out.outf[1] = __floatundisf(ui);  // 15790320.0f -> 0x4B70F0F0
  out.outs = __fixsfdi(fneg);       // -123456 -> 0xFFFFFFFFFFFE1DC0
  out.outu = __fixunssfdi(fpos);    // 123456 -> 0x000000000001E240
  return 0;
}

// F32CONV64: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (24 bytes):
// F32CONV64: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}00 20 F1 C7 F0 F0 70 4B C0 1D FE FF FF FF FF FF
// F32CONV64: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}40 E2 01 00 00 00 00 00
// F32CONV64: "halt":"hlt"
