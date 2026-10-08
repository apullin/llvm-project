// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085 -c %i8085_sdk/sysroot/crt/crt0.S -o %t.crt.o
// RUN: %i8085_clang -target i8085 -O1 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 1000000 -d %testdump_addr:4 %t.bin 2>&1 | FileCheck %s
// RUN: %i8085_clang -target i8085 -Oz -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 1000000 -d %testdump_addr:4 %t.bin 2>&1 | FileCheck %s
volatile unsigned char out[4] __attribute__((section(".testdump")));
void (*volatile callback)(int);
__attribute__((noinline)) void emit(int n) { out[0] += n; }
__attribute__((noinline)) void loop(int n) {
  for (int i = 0; i < n; ++i) {
    if (i)
      callback(i);
    ++out[1];
  }
}
int main(void) {
  callback = emit;
  loop(5);
  out[2] = 0xad;
  out[3] = 0xde;
  return 0;
}
// CHECK: FF00:{{ *}}0A 05 AD DE
// CHECK: "halt":"hlt"
