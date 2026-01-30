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
// RUN:   -d 0x0560:0x04 %t.bin 2>&1 | FileCheck %s --check-prefix=PSW

__attribute__((naked)) int main(void) {
  __asm__ volatile("mvi a, 0xff\n"
                   "adi 0x01\n"
                   "push psw\n"
                   "mvi a, 0x55\n"
                   "ora a\n"
                   "pop psw\n"
                   "sta 0x0560\n"
                   "mvi a, 0x00\n"
                   "jz psw_z_pass\n"
                   "mvi a, 0xee\n"
                   "jmp psw_z_store\n"
                   "psw_z_pass:\n"
                   "mvi a, 0x01\n"
                   "psw_z_store:\n"
                   "sta 0x0561\n"
                   "mvi a, 0x00\n"
                   "jpe psw_p_pass\n"
                   "mvi a, 0xee\n"
                   "jmp psw_p_store\n"
                   "psw_p_pass:\n"
                   "mvi a, 0x01\n"
                   "psw_p_store:\n"
                   "sta 0x0562\n"
                   "mvi a, 0x00\n"
                   "adc a\n"
                   "sta 0x0563\n"
                   "hlt\n");
}

// PSW: Memory dump 0x0560 - 0x0563 (4 bytes):
// PSW: 0560: 00 01 01 01
// PSW: "halt":"hlt"
