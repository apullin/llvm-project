// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -S %s -o - | FileCheck %s

unsigned char test_imm(unsigned char x) {
  asm volatile("adi %0" : : "I"(7));
  return x;
}

unsigned char test_tied(unsigned char x) {
  asm volatile("mov a, %0\n\tadi 1\n\tmov %0, a" : "+r"(x));
  return x;
}

unsigned char stress_regs(unsigned char a, unsigned char b, unsigned char c) {
  unsigned char out;
  asm volatile(
      "mov %0, %1\n\tmov %0, %2\n\tmov %0, %3"
      : "=r"(out)
      : "r"(a), "r"(b), "r"(c));
  return out;
}

// CHECK-LABEL: test_imm:
// CHECK: ;APP
// CHECK: ADI 7
// CHECK: ;NO_APP
// CHECK: RET
// CHECK-LABEL: test_tied:
// CHECK: ;APP
// CHECK: MOV A,
// CHECK: ADI 1
// CHECK: MOV
// CHECK: ;NO_APP
// CHECK: RET
// CHECK-LABEL: stress_regs:
// CHECK: ;APP
// CHECK: MOV
// CHECK: ;NO_APP
// CHECK: RET
