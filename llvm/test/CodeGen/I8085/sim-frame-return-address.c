// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-32kram-32krom.ld -o %t.elf %t.crt0.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 200000 -d 0x0210:0x07 %t.bin 2>&1 | FileCheck %s
// RUN: %i8085_clang -target i8085-unknown-elf -Os -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %s -o %t.os.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-32kram-32krom.ld -o %t.os.elf %t.crt0.o %t.os.o
// RUN: %i8085_objcopy -O binary %t.os.elf %t.os.bin
// RUN: %i8085_sim -S -q -n 200000 -d 0x0210:0x07 %t.os.bin 2>&1 | FileCheck %s

#include <stdint.h>

__attribute__((noinline)) static void touch(volatile uint8_t *frame) {
  frame[3] ^= 0x5a;
}

__attribute__((noinline)) static void probe(void) {
  volatile uint8_t frame[7] = {0};
  volatile uint8_t *out = (volatile uint8_t *)0x0210;
  void *return_before = __builtin_return_address(0);
  void *frame_address = __builtin_frame_address(0);

  frame[3] = 0xa5;
  touch(frame);
  void *return_after = __builtin_return_address(0);
  const volatile uint8_t *anchor = (const volatile uint8_t *)frame_address;
  uint16_t anchored_return =
      (uint16_t)anchor[0] | ((uint16_t)anchor[1] << 8);

  out[0] = return_before != 0;
  out[1] = return_before == return_after;
  out[2] = frame_address != 0;
  out[3] = anchored_return == (uint16_t)(uintptr_t)return_before;
  out[4] = __builtin_return_address(1) == 0;
  out[5] = __builtin_frame_address(1) == 0;
  out[6] = frame[3] == 0xff;
}

int main(void) {
  probe();
  return 0;
}

// CHECK: Memory dump 0x0210 - 0x0216 (7 bytes):
// CHECK: 0210: 01 01 01 01 01 01 01
// CHECK: "halt":"hlt"
