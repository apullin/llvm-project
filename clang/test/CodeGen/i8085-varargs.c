// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -S %s -o - | FileCheck %s

typedef __builtin_va_list va_list;

int sum(int n, ...) {
  va_list ap;
  __builtin_va_start(ap, n);
  int v = __builtin_va_arg(ap, int);
  __builtin_va_end(ap);
  return v;
}

// CHECK-LABEL: sum:
// CHECK: RET
