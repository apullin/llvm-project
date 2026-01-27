// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=i8085-unknown-elf -S %s -o - | FileCheck %s --check-prefix=ASM

struct pair {
  int a;
  int b;
};

struct pair make_pair(int x, int y) {
  struct pair p = {x, y};
  return p;
}

int sum_pair(void) {
  struct pair p = make_pair(1, 2);
  return p.a + p.b;
}

// IR: define{{.*}} void @make_pair{{.*}}sret(%struct.pair)
// ASM-LABEL: sum_pair:
// ASM: CALL make_pair
// ASM: RET
