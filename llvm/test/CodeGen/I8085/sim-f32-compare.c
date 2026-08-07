// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %i8085_llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x20 %t.bin 2>&1 | FileCheck %s --check-prefix=F32CMP
// RUN: %i8085_clang -target i8085-unknown-elf -O1 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.o1.bc
// RUN: %i8085_llc -O1 -mtriple=i8085-unknown-elf -filetype=obj %t.o1.bc -o %t.o1.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -Map %t.o1.map \
// RUN:   -o %t.o1.elf %t.crt0.o %t.o1.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.o1.elf %t.o1.bin
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x20 %t.o1.bin 2>&1 | FileCheck %s --check-prefix=F32CMP
// RUN: %i8085_clang -target i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.os.bc
// RUN: %i8085_llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.os.bc -o %t.os.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -Map %t.os.map \
// RUN:   -o %t.os.elf %t.crt0.o %t.os.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.os.elf %t.os.bin
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x20 %t.os.bin 2>&1 | FileCheck %s --check-prefix=F32CMP
// RUN: %i8085_clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.o2.bc
// RUN: %i8085_llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.o2.bc -o %t.o2.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -Map %t.o2.map \
// RUN:   -o %t.o2.elf %t.crt0.o %t.o2.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.o2.elf %t.o2.bin
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x20 %t.o2.bin 2>&1 | FileCheck %s --check-prefix=F32CMP

#include <stdint.h>

#include "sim-testdump.h"

extern int32_t __eqsf2(float, float);
extern int32_t __nesf2(float, float);
extern int32_t __ltsf2(float, float);
extern int32_t __lesf2(float, float);
extern int32_t __gtsf2(float, float);
extern int32_t __gesf2(float, float);
extern int32_t __unordsf2(float, float);

TESTDUMP_I32(out, 8);

int main(void) {
  volatile float a = 1.5f;
  volatile float b = 2.25f;
  volatile float c = 1.5f;
  union {
    uint32_t u;
    float f;
  } nan = {0x7FC00000u};

  out[0] = __eqsf2(a, c);       // expect 0
  out[1] = __nesf2(a, b);       // expect -1
  out[2] = __ltsf2(a, b);       // expect -1
  out[3] = __lesf2(a, b);       // expect -1
  out[4] = __gtsf2(b, a);       // expect 1
  out[5] = __gesf2(b, a);       // expect 1
  out[6] = __unordsf2(nan.f, a); // expect 1
  out[7] = __unordsf2(a, b);    // expect 0
  return 0;
}

// F32CMP: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (32 bytes):
// F32CMP: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}00 00 00 00 FF FF FF FF FF FF FF FF FF FF FF FF
// F32CMP: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}01 00 00 00 01 00 00 00 01 00 00 00 00 00 00 00
// F32CMP: "halt":"hlt"
