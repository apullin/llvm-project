// RUN: %clang_cc1 -triple i8085-unknown-elf -emit-llvm -o - %s | FileCheck %s
void read_ahead(char *p) {
  __builtin_prefetch(p, 0, 3);
}
// CHECK-LABEL: define{{.*}} @read_ahead
// CHECK: call void @llvm.prefetch.p0(ptr {{.*}}, i32 0, i32 3, i32 1)
