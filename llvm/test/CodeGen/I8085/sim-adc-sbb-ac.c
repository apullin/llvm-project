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
// RUN:   -d 0x05E0:0x08 %t.bin 2>&1 | FileCheck %s --check-prefix=ADCSBB

__attribute__((naked)) int main(void) {
  __asm__ volatile("lxi sp, 0x0600\n"
                   "mvi a, 0x0f\n"
                   "stc\n"
                   "aci 0x00\n"
                   "push psw\n"
                   "lda 0x05ff\n"
                   "sta 0x05e0\n"
                   "lda 0x05fe\n"
                   "sta 0x05e1\n"
                   "lxi sp, 0x0600\n"
                   "mvi a, 0xff\n"
                   "stc\n"
                   "aci 0x00\n"
                   "push psw\n"
                   "lda 0x05ff\n"
                   "sta 0x05e2\n"
                   "lda 0x05fe\n"
                   "sta 0x05e3\n"
                   "lxi sp, 0x0600\n"
                   "mvi a, 0x10\n"
                   "stc\n"
                   "sbi 0x00\n"
                   "push psw\n"
                   "lda 0x05ff\n"
                   "sta 0x05e4\n"
                   "lda 0x05fe\n"
                   "sta 0x05e5\n"
                   "lxi sp, 0x0600\n"
                   "mvi a, 0x00\n"
                   "stc\n"
                   "sbi 0x00\n"
                   "push psw\n"
                   "lda 0x05ff\n"
                   "sta 0x05e6\n"
                   "lda 0x05fe\n"
                   "sta 0x05e7\n"
                   "hlt\n");
}

// ADCSBB: Memory dump 0x05E0 - 0x05E7 (8 bytes):
// ADCSBB: 05E0: 10 12 00 57 0F 16 FF 97
// ADCSBB: "halt":"hlt"
