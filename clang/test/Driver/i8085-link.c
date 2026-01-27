// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -fuse-ld=lld -### %s 2>&1 | \
// RUN:   FileCheck %s --check-prefix=CC1 --check-prefix=LINK
//
// CC1: "-cc1"
// CC1-SAME: "-internal-isystem" "{{.*}}/lib/clang/{{[^" ]*}}/i8085/include"
//
// LINK: "{{.*}}ld.lld"
// LINK-SAME: "-T{{.*}}/lib/clang/{{[^" ]*}}/i8085.ld"
// LINK-SAME: "{{.*}}/lib/clang/{{[^" ]*}}/i8085/lib/crt1.o"
// LINK-SAME: "{{.*}}/lib/clang/{{[^" ]*}}/i8085/lib/crti.o"
// LINK-SAME: "{{.*}}/lib/clang/{{[^" ]*}}/i8085/lib/crtbegin.o"
// LINK-SAME: "-L{{.*}}/lib/clang/{{[^" ]*}}/i8085/lib"
// LINK-SAME: "-lgcc"
// LINK-SAME: "-lc"
// LINK-SAME: "-lgcc"
// LINK-SAME: "{{.*}}/lib/clang/{{[^" ]*}}/i8085/lib/crtend.o"
// LINK-SAME: "{{.*}}/lib/clang/{{[^" ]*}}/i8085/lib/crtn.o"

int main(void) { return 0; }
