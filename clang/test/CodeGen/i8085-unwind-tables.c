// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -funwind-tables -c %s -o %t.o
// RUN: llvm-readelf -S %t.o | FileCheck %s

int foo(int a) { return a + 1; }

// CHECK: .text
// CHECK-NOT: .eh_frame
