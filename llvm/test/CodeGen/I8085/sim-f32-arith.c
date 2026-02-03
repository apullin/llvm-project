// REQUIRES: i8085-sim
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %S/../../../../../sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-sim-flat.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.elf %t.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d %testdump_addr:0x14 %t.bin 2>&1 | FileCheck %s --check-prefix=F32ARITH
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O1 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.o1.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -O1 -mtriple=i8085-unknown-elf -filetype=obj %t.o1.bc -o %t.o1.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-sim-flat.ld -Map %t.o1.map \
// RUN:   -o %t.o1.elf %t.crt0.o %t.o1.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.o1.elf %t.o1.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d %testdump_addr:0x14 %t.o1.bin 2>&1 | FileCheck %s --check-prefix=F32ARITH
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.os.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.os.bc -o %t.os.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-sim-flat.ld -Map %t.os.map \
// RUN:   -o %t.os.elf %t.crt0.o %t.os.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.os.elf %t.os.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d %testdump_addr:0x14 %t.os.bin 2>&1 | FileCheck %s --check-prefix=F32ARITH
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.o2.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.o2.bc -o %t.o2.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-sim-flat.ld -Map %t.o2.map \
// RUN:   -o %t.o2.elf %t.crt0.o %t.o2.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.o2.elf %t.o2.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 -d %testdump_addr:0x14 %t.o2.bin 2>&1 | FileCheck %s --check-prefix=F32ARITH

#include <stdint.h>

#include "sim-testdump.h"

extern float __addsf3(float, float);
extern float __subsf3(float, float);
extern float __mulsf3(float, float);
extern float __divsf3(float, float);
extern float __negsf2(float);

TESTDUMP_F32(out, 5);

int main(void) {
  volatile float a = 1.5f;
  volatile float b = 2.25f;
  volatile float c = 5.5f;
  volatile float d = 7.5f;
  volatile float e = 2.5f;

  out[0] = __addsf3(a, b); // 3.75f -> 0x40700000
  out[1] = __subsf3(c, b); // 3.25f -> 0x40500000
  out[2] = __mulsf3(a, b); // 3.375f -> 0x40580000
  out[3] = __divsf3(d, e); // 3.0f -> 0x40400000
  out[4] = __negsf2(a);    // -1.5f -> 0xBFC00000
  return 0;
}

// F32ARITH: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (20 bytes):
// F32ARITH: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}00 00 70 40 00 00 50 40 00 00 58 40 00 00 40 40
// F32ARITH: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}00 00 C0 BF
// F32ARITH: "halt":"hlt"
