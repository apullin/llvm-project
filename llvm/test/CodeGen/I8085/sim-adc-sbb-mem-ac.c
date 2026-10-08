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
// RUN:   -d 0x0630:0x08 %t.bin 2>&1 | FileCheck %s --check-prefix=ADCMEM

__attribute__((naked)) int main(void) {
  __asm__ volatile("lxi h, 0x0600\n"
                   "mvi m, 0x00\n"
                   "lxi sp, 0x0660\n"
                   "mvi a, 0x0f\n"
                   "stc\n"
                   "adc m\n"
                   "push psw\n"
                   "lda 0x065e\n"
                   "sta 0x0630\n"
                   "lda 0x065f\n"
                   "sta 0x0631\n"
                   "lxi sp, 0x0660\n"
                   "mvi a, 0xff\n"
                   "stc\n"
                   "adc m\n"
                   "push psw\n"
                   "lda 0x065e\n"
                   "sta 0x0632\n"
                   "lda 0x065f\n"
                   "sta 0x0633\n"
                   "lxi sp, 0x0660\n"
                   "mvi a, 0x10\n"
                   "stc\n"
                   "sbb m\n"
                   "push psw\n"
                   "lda 0x065e\n"
                   "sta 0x0634\n"
                   "lda 0x065f\n"
                   "sta 0x0635\n"
                   "lxi sp, 0x0660\n"
                   "mvi a, 0x00\n"
                   "stc\n"
                   "sbb m\n"
                   "push psw\n"
                   "lda 0x065e\n"
                   "sta 0x0636\n"
                   "lda 0x065f\n"
                   "sta 0x0637\n"
                   "hlt\n");
}

// ADCMEM: Memory dump 0x0630 - 0x0637 (8 bytes):
// ADCMEM: 0630: 10 10 55 00 14 0F 95 FF
// ADCMEM: "halt":"hlt"
