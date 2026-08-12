// RUN: %clang_cc1 -triple tms9900 -O2 -flto-linker-scripts -emit-llvm %s -o - | FileCheck %s
// REQUIRES: tms9900-registered-target

volatile int sink;

__attribute__((section(".text.fast"))) int fast(int x) {
  sink = x;
  return sink + 1;
}

int slow(int x) {
  return fast(x) + 1;
}

int _start(void) {
  return slow(sink);
}

// CHECK: @sink = {{.*}} section ".bss"{{.*}} #[[BSS:[0-9]+]]

// CHECK: define {{.*}} @fast{{.*}} #[[FAST:[0-9]+]] section ".text.fast"
// CHECK: define {{.*}} @slow{{.*}} #[[TEXT:[0-9]+]] section ".text"
// CHECK: call i16 @fast
// CHECK: define {{.*}} @_start{{.*}} #[[TEXT]] section ".text"
// CHECK: call i16 @fast

// CHECK: attributes #[[BSS]] = { "linker_input_section"=".bss" }
// CHECK: attributes #[[FAST]] = { {{.*}}"linker_input_section"=".text.fast"
// CHECK: attributes #[[TEXT]] = { {{.*}}"linker_input_section"=".text"
