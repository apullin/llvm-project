// RUN: %clang -### -c %s --target=i8085-unknown-elf 2>&1 | FileCheck %s
// RUN: %clang -### -S %s --target=i8085-unknown-elf 2>&1 | FileCheck %s
//
// CHECK: "-cc1"
// CHECK: "-triple" "i8085-unknown-unknown-elf"

int foo(int x) { return x + 1; }
