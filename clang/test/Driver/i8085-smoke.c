// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -Os -S %s -o - | FileCheck %s
// CHECK: MVI
// CHECK: RET

int foo(void) { return 3; }
int main(void) { return foo(); }
