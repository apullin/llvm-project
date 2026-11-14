// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -mcpu=i8085-undoc -### -c %s 2>&1 | FileCheck %s --check-prefix=DRV
// RUN: %clang --target=i8085-unknown-elf -mcpu=i8085-undoc -dM -E %s | FileCheck %s --check-prefix=DEF

// DRV-NOT: argument unused
// DRV: "-target-cpu" "i8085-undoc"
// DRV: "-target-feature" "+undoc"
// DEF: #define __I8085_UNDOC__ 1

int f(int x) { return x >> 1; }
