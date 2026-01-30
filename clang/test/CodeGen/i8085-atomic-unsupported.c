// REQUIRES: i8085-registered-target
// RUN: not %clang --target=i8085-unknown-elf -c %s 2>&1 | FileCheck %s

int foo(int *p) {
  return __atomic_fetch_add(p, 1, __ATOMIC_SEQ_CST);
}

// CHECK: error: i8085 does not support atomic operations
