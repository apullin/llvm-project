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
// RUN:   -d 0x0530:0x08 %t.bin 2>&1 | FileCheck %s --check-prefix=JCC

__attribute__((naked)) int main(void) {
  __asm__ volatile("xra a\n"
                   "jz jz_pass\n"
                   "mvi a, 0xee\n"
                   "jmp jz_store\n"
                   "jz_pass:\n"
                   "mvi a, 0x01\n"
                   "jz_store:\n"
                   "sta 0x0530\n"
                   "mvi a, 0x01\n"
                   "ora a\n"
                   "jnz jnz_pass\n"
                   "mvi a, 0xee\n"
                   "jmp jnz_store\n"
                   "jnz_pass:\n"
                   "mvi a, 0x01\n"
                   "jnz_store:\n"
                   "sta 0x0531\n"
                   "stc\n"
                   "jc jc_pass\n"
                   "mvi a, 0xee\n"
                   "jmp jc_store\n"
                   "jc_pass:\n"
                   "mvi a, 0x01\n"
                   "jc_store:\n"
                   "sta 0x0532\n"
                   "xra a\n"
                   "jnc jnc_pass\n"
                   "mvi a, 0xee\n"
                   "jmp jnc_store\n"
                   "jnc_pass:\n"
                   "mvi a, 0x01\n"
                   "jnc_store:\n"
                   "sta 0x0533\n"
                   "mvi a, 0x01\n"
                   "ora a\n"
                   "jp jp_pass\n"
                   "mvi a, 0xee\n"
                   "jmp jp_store\n"
                   "jp_pass:\n"
                   "mvi a, 0x01\n"
                   "jp_store:\n"
                   "sta 0x0534\n"
                   "mvi a, 0x80\n"
                   "ora a\n"
                   "jm jm_pass\n"
                   "mvi a, 0xee\n"
                   "jmp jm_store\n"
                   "jm_pass:\n"
                   "mvi a, 0x01\n"
                   "jm_store:\n"
                   "sta 0x0535\n"
                   "xra a\n"
                   "jpe jpe_pass\n"
                   "mvi a, 0xee\n"
                   "jmp jpe_store\n"
                   "jpe_pass:\n"
                   "mvi a, 0x01\n"
                   "jpe_store:\n"
                   "sta 0x0536\n"
                   "mvi a, 0x01\n"
                   "ora a\n"
                   "jpo jpo_pass\n"
                   "mvi a, 0xee\n"
                   "jmp jpo_store\n"
                   "jpo_pass:\n"
                   "mvi a, 0x01\n"
                   "jpo_store:\n"
                   "sta 0x0537\n"
                   "hlt\n");
}

// JCC: Memory dump 0x0530 - 0x0537 (8 bytes):
// JCC: 0530: 01 01 01 01 01 01 01 01
// JCC: "halt":"hlt"
