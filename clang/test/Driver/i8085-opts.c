// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -### -O0 %s 2>&1 | FileCheck %s --check-prefix=O0
// RUN: %clang --target=i8085-unknown-elf -### -O1 %s 2>&1 | FileCheck %s --check-prefix=O1
// RUN: %clang --target=i8085-unknown-elf -### -O2 %s 2>&1 | FileCheck %s --check-prefix=O2
// RUN: %clang --target=i8085-unknown-elf -### -Os %s 2>&1 | FileCheck %s --check-prefix=OS

// O0: "-O0"
// O1: "-O1"
// O2: "-O2"
// OS: "-Os"

int foo(int x) { return x + 1; }
