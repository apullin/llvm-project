// RUN: %clang_cc1 -triple i8085-unknown-elf -emit-llvm -o - %s | FileCheck %s

void *return_address(void) {
  return __builtin_return_address(0);
}

void *frame_address(void) {
  return __builtin_frame_address(0);
}

// CHECK-LABEL: define{{.*}} ptr @return_address()
// CHECK: call ptr @llvm.returnaddress(i32 0)
// CHECK-LABEL: define{{.*}} ptr @frame_address()
// CHECK: call ptr @llvm.frameaddress.p0(i32 0)
