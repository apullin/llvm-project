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
// RUN:   -d 0x0570:0x06 %t.bin 2>&1 | FileCheck %s --check-prefix=LOGIC

__attribute__((naked)) int main(void) {
  __asm__ volatile("lxi sp, 0x0600\n"
                   "mvi a, 0x0f\n"
                   "mvi b, 0xf0\n"
                   "ana b\n"
                   "push psw\n"
                   "lda 0x05fe\n"
                   "sta 0x0570\n"
                   "lda 0x05ff\n"
                   "sta 0x0571\n"
                   "lxi sp, 0x0600\n"
                   "mvi a, 0x0f\n"
                   "mvi b, 0xf0\n"
                   "ora b\n"
                   "push psw\n"
                   "lda 0x05fe\n"
                   "sta 0x0572\n"
                   "lda 0x05ff\n"
                   "sta 0x0573\n"
                   "lxi sp, 0x0600\n"
                   "mvi a, 0x0f\n"
                   "mvi b, 0x0f\n"
                   "xra b\n"
                   "push psw\n"
                   "lda 0x05fe\n"
                   "sta 0x0574\n"
                   "lda 0x05ff\n"
                   "sta 0x0575\n"
                   "hlt\n");
}

// LOGIC: Memory dump 0x0570 - 0x0575 (6 bytes):
// LOGIC: 0570: 54 00 84 FF 44 00
// LOGIC: "halt":"hlt"
