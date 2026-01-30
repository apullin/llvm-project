// REQUIRES: i8085-sim
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %S/../../../../../sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -O2 -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o
// RUN: llvm-objcopy -O binary %t.elf %t.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 \
// RUN:   -d 0x0600:0x0A %t.bin 2>&1 | FileCheck %s --check-prefix=ABI
// NOTE: Exercise sret/byval+varargs under -O2.

#include <stdint.h>
#include <stdarg.h>

struct ByVal {
  uint8_t a;
  uint8_t pad;
  uint16_t b;
  uint16_t c;
};

struct Ret {
  uint16_t r0;
  uint16_t r1;
  uint16_t r2;
  uint16_t r3;
};

#define LOAD_U16(p) ((uint16_t)(p)[0] | ((uint16_t)(p)[1] << 8))

__attribute__((noinline)) struct Ret make_ret(int tag, struct ByVal bv, ...) {
  va_list ap;
  va_start(ap, bv);
  int v0 = va_arg(ap, int);
  int v1 = va_arg(ap, int);
  va_end(ap);

  const uint8_t *bp = (const uint8_t *)&bv;
  uint8_t a = bp[0];
  uint16_t b = LOAD_U16(bp + 2);
  uint16_t c = LOAD_U16(bp + 4);

  struct Ret r;
  r.r0 = (uint16_t)tag;
  r.r1 = (uint16_t)(a + b + c);
  r.r2 = (uint16_t)v0;
  r.r3 = (uint16_t)(v1 + tag);
  return r;
}

__attribute__((noinline)) uint16_t consume_ret(struct Ret r) {
  const uint8_t *rp = (const uint8_t *)&r;
  uint16_t r0 = LOAD_U16(rp + 0);
  uint16_t r1 = LOAD_U16(rp + 2);
  uint16_t r2 = LOAD_U16(rp + 4);
  uint16_t r3 = LOAD_U16(rp + 6);
  return (uint16_t)(r0 ^ r1 ^ r2 ^ r3);
}

int main(void) {
  struct ByVal bv;
  bv.a = 1;
  bv.pad = 0;
  bv.b = 0x1234;
  bv.c = 0x0042;
  struct Ret r = make_ret(0x10, bv, 0x2222, 0x3333);

  const uint8_t *rp = (const uint8_t *)&r;
  uint16_t r0 = LOAD_U16(rp + 0);
  uint16_t r1 = LOAD_U16(rp + 2);
  uint16_t r2 = LOAD_U16(rp + 4);
  uint16_t r3 = LOAD_U16(rp + 6);

  volatile uint16_t *out = (volatile uint16_t *)0x0600;
  out[0] = r0;
  out[1] = r1;
  out[2] = r2;
  out[3] = r3;
  out[4] = consume_ret(r);
  return 0;
}

// ABI: Memory dump 0x0600 - 0x0609 (10 bytes):
// ABI: 0600: 10 00 77 12 22 22 43 33 06 03
// ABI: "halt":"hlt"
