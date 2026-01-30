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
// RUN:   -d 0x0500:0x04 %t.bin 2>&1 | FileCheck %s --check-prefix=INRDCR

__attribute__((naked)) int main(void) {
  __asm__ volatile("mvi a, 0xff\n"
                   "stc\n"
                   "inr a\n"
                   "jz inr_z_ok\n"
                   "mvi a, 0xee\n"
                   "inr_z_ok:\n"
                   "sta 0x0500\n"
                   "mvi a, 0x00\n"
                   "adc a\n"
                   "sta 0x0501\n"
                   "mvi a, 0x00\n"
                   "stc\n"
                   "dcr a\n"
                   "jm dcr_s_ok\n"
                   "mvi a, 0xee\n"
                   "dcr_s_ok:\n"
                   "sta 0x0502\n"
                   "mvi a, 0x00\n"
                   "adc a\n"
                   "sta 0x0503\n"
                   "hlt\n");
}

// INRDCR: Memory dump 0x0500 - 0x0503 (4 bytes):
// INRDCR: 0500: 00 01 FF 01
// INRDCR: "halt":"hlt"
