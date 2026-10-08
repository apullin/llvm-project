// REQUIRES: i8085-registered-target
// RUN: not %clang --target=i8085-unknown-elf -S %s -o /dev/null 2>&1 | FileCheck %s
void indirect(unsigned char *p) {
  __asm__ volatile("lda %0" : : "m"(*p) : "a");
}
// CHECK: error: invalid operand in inline asm
