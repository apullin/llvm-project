// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -O0 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s
// RUN: %clang --target=i8085-unknown-elf -O2 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s
unsigned char bytes[4];

void barriers(unsigned char *p) {
  unsigned char local = 1;
  __asm__ volatile("" : "+m"(*p), "+m"(local), "+m"(bytes[0]));
  bytes[1] = local;
}

unsigned char absolute(void) {
  unsigned char result;
  __asm__ volatile("lda %1\n\tmov %0, a"
                   : "=r"(result) : "m"(bytes[2]) : "a");
  return result;
}
// CHECK-LABEL: barriers:
// CHECK: ;APP
// CHECK: ;NO_APP
// CHECK-LABEL: absolute:
// CHECK: LDA bytes+2
// CHECK: RET
