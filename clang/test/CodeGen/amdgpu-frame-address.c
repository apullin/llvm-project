// REQUIRES: amdgpu-registered-target
// RUN: %clang_cc1 -O2 -triple amdgcn-amd-amdhsa -emit-llvm %s -o - | FileCheck %s

// The frame address is a pointer in the alloca address space (address
// space 5 on AMDGPU). The builtin returns a generic void *, so it is the
// address space cast of that pointer.

// CHECK-LABEL: define {{.*}}ptr @frame(
// CHECK: [[TMP0:%.*]] = {{.*}}call ptr addrspace(5) @llvm.frameaddress.p5(i32 0)
// CHECK-NEXT: [[TMP1:%.*]] = addrspacecast ptr addrspace(5) [[TMP0]] to ptr
// CHECK-NEXT: ret ptr [[TMP1]]
void *frame(void) { return __builtin_frame_address(0); }

// CHECK-LABEL: define {{.*}}ptr @caller_frame(
// CHECK: [[TMP0:%.*]] = {{.*}}call ptr addrspace(5) @llvm.frameaddress.p5(i32 1)
// CHECK-NEXT: [[TMP1:%.*]] = addrspacecast ptr addrspace(5) [[TMP0]] to ptr
// CHECK-NEXT: ret ptr [[TMP1]]
void *caller_frame(void) { return __builtin_frame_address(1); }
