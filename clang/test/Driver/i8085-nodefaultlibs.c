// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -nodefaultlibs -### %s 2>&1 | FileCheck %s
// CHECK: "{{.*}}ld.lld"
// CHECK: crt1.o
// CHECK: crti.o
// CHECK: crtbegin.o
// CHECK-NOT: "-lc"
// CHECK-NOT: "-lgcc"
// CHECK: crtend.o
// CHECK: crtn.o

int main(void) { return 0; }
