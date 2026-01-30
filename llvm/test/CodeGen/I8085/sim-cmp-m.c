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
// RUN:   -d 0x0590:0x07 %t.bin 2>&1 | FileCheck %s --check-prefix=CMPM

__attribute__((naked)) int main(void) {
  __asm__ volatile("lxi h, 0x0600\n"
                   "mvi a, 0x10\n"
                   "sta 0x0600\n"
                   "mvi a, 0x10\n"
                   "cmp m\n"
                   "sta 0x0590\n"
                   "jz cmp_eq_z_ok\n"
                   "mvi a, 0xee\n"
                   "jmp cmp_eq_z_store\n"
                   "cmp_eq_z_ok:\n"
                   "mvi a, 0x01\n"
                   "cmp_eq_z_store:\n"
                   "sta 0x0591\n"
                   "jnc cmp_eq_c_ok\n"
                   "mvi a, 0xee\n"
                   "jmp cmp_eq_c_store\n"
                   "cmp_eq_c_ok:\n"
                   "mvi a, 0x01\n"
                   "cmp_eq_c_store:\n"
                   "sta 0x0592\n"
                   "mvi a, 0x20\n"
                   "sta 0x0600\n"
                   "mvi a, 0x10\n"
                   "cmp m\n"
                   "sta 0x0593\n"
                   "jc cmp_lt_c_ok\n"
                   "mvi a, 0xee\n"
                   "jmp cmp_lt_c_store\n"
                   "cmp_lt_c_ok:\n"
                   "mvi a, 0x01\n"
                   "cmp_lt_c_store:\n"
                   "sta 0x0594\n"
                   "jm cmp_lt_s_ok\n"
                   "mvi a, 0xee\n"
                   "jmp cmp_lt_s_store\n"
                   "cmp_lt_s_ok:\n"
                   "mvi a, 0x01\n"
                   "cmp_lt_s_store:\n"
                   "sta 0x0595\n"
                   "jnz cmp_lt_z_ok\n"
                   "mvi a, 0xee\n"
                   "jmp cmp_lt_z_store\n"
                   "cmp_lt_z_ok:\n"
                   "mvi a, 0x01\n"
                   "cmp_lt_z_store:\n"
                   "sta 0x0596\n"
                   "hlt\n");
}

// CMPM: Memory dump 0x0590 - 0x0596 (7 bytes):
// CMPM: 0590: 10 01 01 10 01 01 01
// CMPM: "halt":"hlt"
