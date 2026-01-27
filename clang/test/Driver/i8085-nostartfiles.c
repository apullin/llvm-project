// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -nostartfiles -### %s 2>&1 | FileCheck %s
// CHECK: "{{.*}}ld.lld"
// CHECK-NOT: crt1.o
// CHECK-NOT: crti.o
// CHECK-NOT: crtbegin.o
// CHECK: "-lc"
// CHECK: "-lgcc"
// CHECK-NOT: crtend.o
// CHECK-NOT: crtn.o

int main(void) { return 0; }
