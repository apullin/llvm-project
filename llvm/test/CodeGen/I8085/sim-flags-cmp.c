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
// RUN:   -d 0x0510:0x08 %t.bin 2>&1 | FileCheck %s --check-prefix=CMP

__attribute__((naked)) int main(void) {
  __asm__ volatile("mvi a, 0x10\n"
                   "cmp a\n"
                   "jz cmp1_pass\n"
                   "mvi a, 0xee\n"
                   "jmp cmp1_store\n"
                   "cmp1_pass:\n"
                   "mvi a, 0x01\n"
                   "cmp1_store:\n"
                   "sta 0x0510\n"
                   "mvi a, 0x00\n"
                   "adc a\n"
                   "jz cmp1_cy_pass\n"
                   "mvi a, 0xee\n"
                   "jmp cmp1_cy_store\n"
                   "cmp1_cy_pass:\n"
                   "mvi a, 0x01\n"
                   "cmp1_cy_store:\n"
                   "sta 0x0511\n"
                   "mvi a, 0x10\n"
                   "cpi 0x20\n"
                   "jc cmp2_c_pass\n"
                   "mvi a, 0xee\n"
                   "jmp cmp2_c_store\n"
                   "cmp2_c_pass:\n"
                   "mvi a, 0x01\n"
                   "cmp2_c_store:\n"
                   "sta 0x0512\n"
                   "mvi a, 0x00\n"
                   "jm cmp2_s_pass\n"
                   "mvi a, 0xee\n"
                   "jmp cmp2_s_store\n"
                   "cmp2_s_pass:\n"
                   "mvi a, 0x01\n"
                   "cmp2_s_store:\n"
                   "sta 0x0513\n"
                   "mvi a, 0x00\n"
                   "jnz cmp2_z_pass\n"
                   "mvi a, 0xee\n"
                   "jmp cmp2_z_store\n"
                   "cmp2_z_pass:\n"
                   "mvi a, 0x01\n"
                   "cmp2_z_store:\n"
                   "sta 0x0514\n"
                   "mvi a, 0x20\n"
                   "cpi 0x10\n"
                   "jnc cmp3_c_pass\n"
                   "mvi a, 0xee\n"
                   "jmp cmp3_c_store\n"
                   "cmp3_c_pass:\n"
                   "mvi a, 0x01\n"
                   "cmp3_c_store:\n"
                   "sta 0x0515\n"
                   "mvi a, 0x00\n"
                   "jnz cmp3_z_pass\n"
                   "mvi a, 0xee\n"
                   "jmp cmp3_z_store\n"
                   "cmp3_z_pass:\n"
                   "mvi a, 0x01\n"
                   "cmp3_z_store:\n"
                   "sta 0x0516\n"
                   "mvi a, 0x00\n"
                   "jp cmp3_s_pass\n"
                   "mvi a, 0xee\n"
                   "jmp cmp3_s_store\n"
                   "cmp3_s_pass:\n"
                   "mvi a, 0x01\n"
                   "cmp3_s_store:\n"
                   "sta 0x0517\n"
                   "hlt\n");
}

// CMP: Memory dump 0x0510 - 0x0517 (8 bytes):
// CMP: 0510: 01 01 01 01 01 01 01 01
// CMP: "halt":"hlt"
