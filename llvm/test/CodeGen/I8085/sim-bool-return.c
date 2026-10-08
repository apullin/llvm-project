// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085 -c %i8085_sdk/sysroot/crt/crt0.S -o %t.crt.o
// RUN: %i8085_clang -target i8085 -O0 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 1000000 -d %testdump_addr:8 %t.bin 2>&1 | FileCheck %s
// RUN: %i8085_clang -target i8085 -O2 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 1000000 -d %testdump_addr:8 %t.bin 2>&1 | FileCheck %s
volatile unsigned char out[8] __attribute__((section(".testdump")));
_Bool choose(_Bool *a, _Bool b) __attribute__((noinline));
_Bool choose(_Bool *a, _Bool b) { return *a <= b ? b : *a; }
_Bool bits(_Bool *a, _Bool b) __attribute__((noinline));
_Bool bits(_Bool *a, _Bool b) { return *a | b; }
int main(void) {
  unsigned char n = 0;
  for (int i = 0; i != 2; ++i)
    for (int j = 0; j != 2; ++j) {
      _Bool a = i;
      out[n++] = choose(&a, j);
      out[n++] = bits(&a, j);
    }
  return 0;
}
// CHECK: FF00:{{ *}}00 00 01 01 01 01 01 01
// CHECK: "halt":"hlt"
