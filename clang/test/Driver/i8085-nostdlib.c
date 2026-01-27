// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -nostdlib -### %s 2>&1 | FileCheck %s
// CHECK: "{{.*}}ld.lld"
// CHECK-NOT: crt1.o
// CHECK-NOT: crti.o
// CHECK-NOT: crtbegin.o
// CHECK-NOT: crtend.o
// CHECK-NOT: crtn.o
// CHECK-NOT: "-lc"
// CHECK-NOT: "-lgcc"

int main(void) { return 0; }
