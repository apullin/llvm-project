// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085 -c %i8085_sdk/sysroot/crt/crt0.S -o %t.crt.o
// RUN: %i8085_clang -target i8085 -O0 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 100000 -d %testdump_addr:2 %t.bin 2>&1 | FileCheck %s
// RUN: %i8085_clang -target i8085 -O2 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 100000 -d %testdump_addr:2 %t.bin 2>&1 | FileCheck %s
unsigned char bytes[4] = {1, 2, 3, 4};
volatile unsigned char out[2] __attribute__((section(".testdump")));
int main(void) {
  unsigned char result;
  unsigned char local = 9;
  __asm__ volatile("" : "+m"(local), "+m"(bytes[0]));
  __asm__ volatile("lda %1\n\tmov %0, a"
                   : "=r"(result) : "m"(bytes[2]) : "a");
  out[0] = result;
  out[1] = local;
  return 0;
}
// CHECK: FF00:{{ *}}03 09
// CHECK: "halt":"hlt"
