// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -Os -emit-llvm -S %s -o - | FileCheck %s --check-prefix=IR-OS
// RUN: %clang --target=i8085-unknown-elf -Os -emit-llvm -S %s -o - | \
// RUN:   llc -mtriple=i8085-unknown-elf -mattr=i8085,sram -verify-machineinstrs | \
// RUN:   FileCheck %s --check-prefix=ASM-OS
// RUN: %clang --target=i8085-unknown-elf -Oz -emit-llvm -S %s -o - | FileCheck %s --check-prefix=IR-OZ
// RUN: %clang --target=i8085-unknown-elf -Oz -emit-llvm -S %s -o - | \
// RUN:   llc -mtriple=i8085-unknown-elf -mattr=i8085,sram -verify-machineinstrs | \
// RUN:   FileCheck %s --check-prefix=ASM-OZ

int add(int a, int b) { return a + b; }
__attribute__((minsize)) int add_small(int a, int b) { return a + b; }

// IR-OS: ; Function Attrs:{{.*}}optsize
// IR-OS-NEXT: define{{.*}}@add
// IR-OS: ; Function Attrs:{{.*}}minsize
// IR-OS-NEXT: define{{.*}}@add_small
// IR-OZ: ; Function Attrs:{{.*}}minsize
// IR-OZ-NEXT: define{{.*}}@add
// IR-OZ: ; Function Attrs:{{.*}}minsize
// IR-OZ-NEXT: define{{.*}}@add_small

// ASM-OS-LABEL: add:
// ASM-OS: RET
// ASM-OS-LABEL: add_small:
// ASM-OS: RET

// ASM-OZ-LABEL: add:
// ASM-OZ: RET
// ASM-OZ-LABEL: add_small:
// ASM-OZ: RET
