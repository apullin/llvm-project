// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -c \
// RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
// RUN: %i8085_clang -target i8085-unknown-elf -O0 -ffreestanding -fno-builtin -nostdlib -emit-llvm -c \
// RUN:   %s -o %t.bc
// RUN: %i8085_llc -mtriple=i8085-unknown-elf -filetype=obj %t.bc -o %t.o
// RUN: %i8085_lld -m i8085elf \
// RUN:   -T %i8085_sdk/sysroot/ldscripts/i8085-32kram-32krom.ld -Map %t.map \
// RUN:   -o %t.elf %t.crt0.o %t.o
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 200000 \
// RUN:   -d 0x0550:0x06 %t.bin 2>&1 | FileCheck %s --check-prefix=PCHL

__attribute__((naked)) int main(void) {
  __asm__ volatile("lxi h, pchl_target\n"
                   "pchl\n"
                   "mvi a, 0xee\n"
                   "sta 0x0550\n"
                   "jmp pchl_done\n"
                   "pchl_target:\n"
                   "mvi a, 0x01\n"
                   "sta 0x0550\n"
                   "pchl_done:\n"
                   "lxi h, 0x0600\n"
                   "sphl\n"
                   "mvi a, 0xaa\n"
                   "push psw\n"
                   "lda 0x05ff\n"
                   "sta 0x0551\n"
                   "shld 0x0552\n"
                   "hlt\n");
}

// PCHL: Memory dump 0x0550 - 0x0555 (6 bytes):
// PCHL: 0550: 01 AA 00 06 00 00
// PCHL: "halt":"hlt"
