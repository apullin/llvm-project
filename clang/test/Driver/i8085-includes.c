// REQUIRES: i8085-registered-target
// RUN: %clang -### -E %s --target=i8085-unknown-elf 2>&1 | FileCheck %s --check-prefix=DEFAULT
// RUN: %clang -### -E %s --target=i8085-unknown-elf -nostdinc 2>&1 | FileCheck %s --check-prefix=NOINC
// RUN: %clang -### -E %s --target=i8085-unknown-elf -nostdlibinc 2>&1 | FileCheck %s --check-prefix=NOINC
//
// DEFAULT: "-internal-isystem" "{{.*}}/lib/clang/{{[^" ]*}}/i8085/include"
// NOINC-NOT: "-internal-isystem" "{{.*}}/lib/clang/{{[^" ]*}}/i8085/include"

int foo(void) { return 0; }
