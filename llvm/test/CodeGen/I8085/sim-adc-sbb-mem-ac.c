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
// ADCMEM: 0630: 12 10 57 00 16 0F 97 FF
// ADCMEM: "halt":"hlt"
