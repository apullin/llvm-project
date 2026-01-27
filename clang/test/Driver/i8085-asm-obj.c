// RUN: %clang --target=i8085-unknown-elf -S %s -o %t.s
// RUN: llvm-mc -triple=i8085 -filetype=obj %t.s -o %t.o
// RUN: llvm-objdump -h %t.o | FileCheck %s
//
// CHECK: .text

int add(int a, int b) { return a + b; }
int main(void) { return add(1, 2); }
