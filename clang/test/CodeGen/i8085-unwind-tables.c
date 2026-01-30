// REQUIRES: i8085-registered-target
// RUN: not %clang --target=i8085-unknown-elf -funwind-tables -c %s 2>&1 | FileCheck %s

int foo(int a) { return a + 1; }

// CHECK: error: unsupported option '-funwind-tables' for target
