// REQUIRES: i8085-registered-target, lld
// RUN: %clang --target=i8085-unknown-elf -fuse-ld=lld %s -o %t
// RUN: test -f %t
// RUN: llvm-objdump -h %t | FileCheck %s
// CHECK: .text
// CHECK: .data
// CHECK: .bss

int data = 1;
int bss;
int main(void) { return data + bss; }
