// REQUIRES: i8085-sim
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %S/../../../../../sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %S/../../../../../tooling/build/build-clang-8085/bin/ld.lld -m i8085elf \
// RUN:   -T %S/../../../../../sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o
// RUN: llvm-objcopy -O binary %t.elf %t.bin
// RUN: %S/../../../../../i8085-trace/build/i8085-trace -S -q -n 200000 \
// RUN:   --sid=1 --irq=75@0 --irq=65@0 --irq=55@0 -d 0x0330:0x03 %t.bin 2>&1 | FileCheck %s --check-prefix=RIM

__attribute__((naked)) int main(void) {
  __asm__ volatile("di\n"
                   "rim\n"
                   "sta 0x0330\n"
                   "mvi a, 0x0f\n"
                   "sim\n"
                   "rim\n"
                   "sta 0x0331\n"
                   "mvi a, 0x10\n"
                   "sim\n"
                   "rim\n"
                   "sta 0x0332\n"
                   "ret\n");
}

// RIM: Memory dump 0x0330 - 0x0332 (3 bytes):
// RIM: 0330: F0 F7 B7
// RIM: "halt":"hlt"
