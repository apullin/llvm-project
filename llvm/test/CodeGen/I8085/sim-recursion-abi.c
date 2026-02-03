// REQUIRES: i8085-sim
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %S/../../../../../sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.elf %t.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 2000000 -d 0x0202:0x4 %t.bin 2>&1 | FileCheck %s --check-prefix=RECURSE
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O1 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.o1.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -O1 -mtriple=i8085-unknown-elf -filetype=obj %t.o1.bc -o %t.o1.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.o1.map \
// RUN:   -o %t.o1.elf %t.crt0.o %t.o1.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.o1.elf %t.o1.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 2000000 -d 0x0202:0x4 %t.o1.bin 2>&1 | FileCheck %s --check-prefix=RECURSE
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.os.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.os.bc -o %t.os.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.os.map \
// RUN:   -o %t.os.elf %t.crt0.o %t.os.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.os.elf %t.os.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 2000000 -d 0x0202:0x4 %t.os.bin 2>&1 | FileCheck %s --check-prefix=RECURSE
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.o2.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.o2.bc -o %t.o2.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.o2.map \
// RUN:   -o %t.o2.elf %t.crt0.o %t.o2.o %S/../../../../../sysroot/lib/libgcc.a
// RUN: llvm-objcopy -O binary %t.o2.elf %t.o2.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 2000000 -d 0x0202:0x4 %t.o2.bin 2>&1 | FileCheck %s --check-prefix=RECURSE

#include <stdint.h>

__attribute__((noinline)) static uint16_t rec_u16(uint16_t n) {
  if (n == 0) {
    return 1;
  }
  return (uint16_t)(n + rec_u16((uint16_t)(n - 1)));
}

__attribute__((noinline)) static int16_t rec_i16(int16_t n) {
  if (n <= 0) {
    return 1;
  }
  return (int16_t)(n + rec_i16((int16_t)(n - 1)));
}

int main(void) {
  volatile uint16_t *out = (uint16_t *)0x0202;
  out[0] = rec_u16(20);  // 1 + sum(1..20) = 211 (0x00D3)
  out[1] = (uint16_t)rec_i16(20);
  return 0;
}

// RECURSE: Memory dump 0x0202 - 0x0205 (4 bytes):
// RECURSE: 0202: D3 00 D3 00
// RECURSE: "halt":"hlt"
