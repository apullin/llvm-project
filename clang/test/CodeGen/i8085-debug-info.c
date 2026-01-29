// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -g -c %s -o %t.o
// RUN: llvm-readelf -S %t.o | FileCheck %s

int foo(int a) { return a + 1; }
int bar(int a, int b) { return a - b; }

// CHECK-DAG: .debug_abbrev
// CHECK-DAG: .debug_info
// CHECK-DAG: .debug_line
// CHECK-DAG: .debug_str
// CHECK-DAG: .rela.debug_info
// CHECK-DAG: .rela.debug_line
