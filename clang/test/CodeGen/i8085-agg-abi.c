// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR

struct pair {
  int a;
  int b;
};

int use_pair(struct pair p) {
  return p.a + p.b;
}

struct pair make_pair(int x, int y) {
  struct pair p = {x, y};
  return p;
}

int call_pair(void) {
  struct pair p = make_pair(1, 2);
  return use_pair(p);
}

// IR: define{{.*}} i16 @use_pair(ptr noundef byval(%struct.pair)
// IR: define{{.*}} void @make_pair(ptr {{.*}}sret(%struct.pair)
// IR: call void @make_pair(ptr {{.*}}sret(%struct.pair)
// IR: call i16 @use_pair(ptr {{.*}}byval(%struct.pair)
