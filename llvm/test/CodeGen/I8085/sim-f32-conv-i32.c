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
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x10 %t.bin 2>&1 | FileCheck %s --check-prefix=F32CONV32
// RUN: %i8085_clang -target i8085-unknown-elf -O1 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.o1.bc
// RUN: %i8085_llc -O1 -mtriple=i8085-unknown-elf -filetype=obj %t.o1.bc -o %t.o1.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -Map %t.o1.map \
// RUN:   -o %t.o1.elf %t.crt0.o %t.o1.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.o1.elf %t.o1.bin
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x10 %t.o1.bin 2>&1 | FileCheck %s --check-prefix=F32CONV32
// RUN: %i8085_clang -target i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.os.bc
// RUN: %i8085_llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.os.bc -o %t.os.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -Map %t.os.map \
// RUN:   -o %t.os.elf %t.crt0.o %t.os.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.os.elf %t.os.bin
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x10 %t.os.bin 2>&1 | FileCheck %s --check-prefix=F32CONV32
// RUN: %i8085_clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.o2.bc
// RUN: %i8085_llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.o2.bc -o %t.o2.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -Map %t.o2.map \
// RUN:   -o %t.o2.elf %t.crt0.o %t.o2.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.o2.elf %t.o2.bin
// RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x10 %t.o2.bin 2>&1 | FileCheck %s --check-prefix=F32CONV32

#include <stdint.h>

#include "sim-testdump.h"

extern float __floatsisf(int32_t);
extern float __floatunsisf(uint32_t);
extern int32_t __fixsfsi(float);
extern uint32_t __fixunssfsi(float);

struct TestDump {
  float outf[2];
  int32_t outs;
  uint32_t outu;
} __attribute__((packed));

volatile struct TestDump out TESTDUMP_SECTION TESTDUMP_ALIGN;

int main(void) {
  int32_t si = -123456;
  uint32_t ui = 0x00F0F0F0u;
  float fpos = 123456.0f;
  float fneg = -123456.0f;

  out.outf[0] = __floatsisf(si);   // -123456.0f -> 0xC7F12000
  out.outf[1] = __floatunsisf(ui); // 15790320.0f -> 0x4B70F0F0
  out.outs = __fixsfsi(fneg);      // -123456 -> 0xFFFE1DC0
  out.outu = __fixunssfsi(fpos);   // 123456 -> 0x0001E240
  return 0;
}

// F32CONV32: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (16 bytes):
// F32CONV32: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}00 20 F1 C7 F0 F0 70 4B C0 1D FE FF 40 E2 01 00
// F32CONV32: "halt":"hlt"
