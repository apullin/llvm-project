// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -flto=full -fuse-ld=lld -### %s 2>&1 | FileCheck %s --check-prefix=FULL
// RUN: %clang --target=i8085-unknown-elf -flto=thin -fuse-ld=lld -### %s 2>&1 | FileCheck %s --check-prefix=THIN
// FULL-NOT: unable to pass LLVM bit-code
// FULL: "-cc1"
// FULL-SAME: "-flto=full"
// FULL: "{{.*}}ld.lld"
// THIN-NOT: unable to pass LLVM bit-code
// THIN: "-cc1"
// THIN-SAME: "-flto=thin"
// THIN: "{{.*}}ld.lld"
int main(void) { return 0; }
