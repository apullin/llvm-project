// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -O0 -S %s -o - | FileCheck %s --check-prefix=O0
// RUN: %clang --target=i8085-unknown-elf -O1 -S %s -o - | FileCheck %s --check-prefix=O1
// RUN: %clang --target=i8085-unknown-elf -O2 -S %s -o - | FileCheck %s --check-prefix=O2
// RUN: %clang --target=i8085-unknown-elf -Os -S %s -o - | FileCheck %s --check-prefix=OS

int add(int a, int b) { return a + b; }

// O0-LABEL: add:
// O0: RET
// O1-LABEL: add:
// O1: RET
// O2-LABEL: add:
// O2: RET
// OS-LABEL: add:
// OS: RET
