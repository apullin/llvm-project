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
// RUN:   -d 0x0640:0x08 %t.bin 2>&1 | FileCheck %s --check-prefix=MPAR

__attribute__((naked)) int main(void) {
  __asm__ volatile("lxi h, 0x0600\n"
                   "mvi m, 0x01\n"
                   "lxi sp, 0x0680\n"
                   "mvi a, 0x01\n"
                   "add m\n"
                   "push psw\n"
                   "lda 0x067e\n"
                   "sta 0x0640\n"
                   "lda 0x067f\n"
                   "sta 0x0641\n"
                   "mvi m, 0x02\n"
                   "lxi sp, 0x0680\n"
                   "mvi a, 0x01\n"
                   "add m\n"
                   "push psw\n"
                   "lda 0x067e\n"
                   "sta 0x0642\n"
                   "lda 0x067f\n"
                   "sta 0x0643\n"
                   "mvi m, 0x01\n"
                   "lxi sp, 0x0680\n"
                   "mvi a, 0x02\n"
                   "sub m\n"
                   "push psw\n"
                   "lda 0x067e\n"
                   "sta 0x0644\n"
                   "lda 0x067f\n"
                   "sta 0x0645\n"
                   "mvi m, 0x02\n"
                   "lxi sp, 0x0680\n"
                   "mvi a, 0x01\n"
                   "sub m\n"
                   "push psw\n"
                   "lda 0x067e\n"
                   "sta 0x0646\n"
                   "lda 0x067f\n"
                   "sta 0x0647\n"
                   "hlt\n");
}

// MPAR: Memory dump 0x0640 - 0x0647 (8 bytes):
// MPAR: 0640: 00 02 04 03 00 01 95 FF
// MPAR: "halt":"hlt"
