// REQUIRES: i8085-registered-target
// RUN: not %clang -target i8085-unknown-elf -fexceptions -c %s 2>&1 | FileCheck %s --check-prefix=EXC
// RUN: not %clang -target i8085-unknown-elf -fcxx-exceptions -c %s 2>&1 | FileCheck %s --check-prefix=CXX
// RUN: not %clang -target i8085-unknown-elf -fobjc-exceptions -c %s 2>&1 | FileCheck %s --check-prefix=OBJ

int x;

// EXC: error: unsupported option '-fexceptions' for target
// CXX: error: unsupported option '-fcxx-exceptions' for target
// OBJ: error: unsupported option '-fobjc-exceptions' for target
