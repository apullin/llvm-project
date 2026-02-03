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
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 2000000 -d %testdump_addr:0x10 %t.bin 2>&1 | FileCheck %s --check-prefix=SHU64
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O1 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.o1.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -O1 -mtriple=i8085-unknown-elf -filetype=obj %t.o1.bc -o %t.o1.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-sim-flat.ld -Map %t.o1.map \
// RUN:   -o %t.o1.elf %t.crt0.o %t.o1.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.o1.elf %t.o1.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 2000000 -d %testdump_addr:0x10 %t.o1.bin 2>&1 | FileCheck %s --check-prefix=SHU64
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.os.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.os.bc -o %t.os.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-sim-flat.ld -Map %t.os.map \
// RUN:   -o %t.os.elf %t.crt0.o %t.os.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.os.elf %t.os.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 2000000 -d %testdump_addr:0x10 %t.os.bin 2>&1 | FileCheck %s --check-prefix=SHU64
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.o2.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.o2.bc -o %t.o2.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-sim-flat.ld -Map %t.o2.map \
// RUN:   -o %t.o2.elf %t.crt0.o %t.o2.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.o2.elf %t.o2.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 2000000 -d %testdump_addr:0x10 %t.o2.bin 2>&1 | FileCheck %s --check-prefix=SHU64

#include <stdint.h>

#include "sim-testdump.h"

TESTDUMP_U64(out, 2);

int main(void) {
  uint64_t v = 0x8123456789ABCDEFULL;
  uint64_t acc_l = 0;
  uint64_t acc_r = 0;
  for (uint8_t s = 0; s < 64; ++s) {
    acc_l ^= (uint64_t)(v << s);
    acc_r ^= (uint64_t)(v >> s);
  }
  out[0] = acc_l;
  out[1] = acc_r;
  return 0;
}

// SHU64: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (16 bytes):
// SHU64: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}A5 44 99 78 DD 3C E1 80 B5 76 CD 0E 45 86 3D FE
// SHU64: "halt":"hlt"
