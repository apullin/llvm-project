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
// RUN:   -d 0x05B0:0x08 %t.bin 2>&1 | FileCheck %s --check-prefix=JFALSE

__attribute__((naked)) int main(void) {
  __asm__ volatile(
      "mvi a, 0x01\n"
      "ora a\n"
      "jz jz_bad\n"
      "mvi a, 0x01\n"
      "sta 0x05B0\n"
      "jmp jz_done\n"
      "jz_bad:\n"
      "mvi a, 0xee\n"
      "sta 0x05B0\n"
      "jz_done:\n"
      "xra a\n"
      "jnz jnz_bad\n"
      "mvi a, 0x01\n"
      "sta 0x05B1\n"
      "jmp jnz_done\n"
      "jnz_bad:\n"
      "mvi a, 0xee\n"
      "sta 0x05B1\n"
      "jnz_done:\n"
      "xra a\n"
      "jc jc_bad\n"
      "mvi a, 0x01\n"
      "sta 0x05B2\n"
      "jmp jc_done\n"
      "jc_bad:\n"
      "mvi a, 0xee\n"
      "sta 0x05B2\n"
      "jc_done:\n"
      "stc\n"
      "jnc jnc_bad\n"
      "mvi a, 0x01\n"
      "sta 0x05B3\n"
      "jmp jnc_done\n"
      "jnc_bad:\n"
      "mvi a, 0xee\n"
      "sta 0x05B3\n"
      "jnc_done:\n"
      "mvi a, 0x01\n"
      "ora a\n"
      "jm jm_bad\n"
      "mvi a, 0x01\n"
      "sta 0x05B4\n"
      "jmp jm_done\n"
      "jm_bad:\n"
      "mvi a, 0xee\n"
      "sta 0x05B4\n"
      "jm_done:\n"
      "mvi a, 0x80\n"
      "ora a\n"
      "jp jp_bad\n"
      "mvi a, 0x01\n"
      "sta 0x05B5\n"
      "jmp jp_done\n"
      "jp_bad:\n"
      "mvi a, 0xee\n"
      "sta 0x05B5\n"
      "jp_done:\n"
      "mvi a, 0x01\n"
      "ora a\n"
      "jpe jpe_bad\n"
      "mvi a, 0x01\n"
      "sta 0x05B6\n"
      "jmp jpe_done\n"
      "jpe_bad:\n"
      "mvi a, 0xee\n"
      "sta 0x05B6\n"
      "jpe_done:\n"
      "xra a\n"
      "jpo jpo_bad\n"
      "mvi a, 0x01\n"
      "sta 0x05B7\n"
      "jmp jpo_done\n"
      "jpo_bad:\n"
      "mvi a, 0xee\n"
      "sta 0x05B7\n"
      "jpo_done:\n"
      "hlt\n");
}

// JFALSE: Memory dump 0x05B0 - 0x05B7 (8 bytes):
// JFALSE: 05B0: 01 01 01 01 01 01 01 01
// JFALSE: "halt":"hlt"
