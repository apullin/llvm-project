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
// RUN:   -d 0x05D0:0x08 %t.bin 2>&1 | FileCheck %s --check-prefix=RFALSE

__attribute__((naked)) void test_rz_false(void) {
  __asm__ volatile("mvi a, 0x01\n"
                   "ora a\n"
                   "rz\n"
                   "mvi a, 0x01\n"
                   "sta 0x05D0\n"
                   "ret\n");
}

__attribute__((naked)) void test_rnz_false(void) {
  __asm__ volatile("xra a\n"
                   "rnz\n"
                   "mvi a, 0x01\n"
                   "sta 0x05D1\n"
                   "ret\n");
}

__attribute__((naked)) void test_rc_false(void) {
  __asm__ volatile("xra a\n"
                   "rc\n"
                   "mvi a, 0x01\n"
                   "sta 0x05D2\n"
                   "ret\n");
}

__attribute__((naked)) void test_rnc_false(void) {
  __asm__ volatile("stc\n"
                   "rnc\n"
                   "mvi a, 0x01\n"
                   "sta 0x05D3\n"
                   "ret\n");
}

__attribute__((naked)) void test_rp_false(void) {
  __asm__ volatile("mvi a, 0x80\n"
                   "ora a\n"
                   "rp\n"
                   "mvi a, 0x01\n"
                   "sta 0x05D4\n"
                   "ret\n");
}

__attribute__((naked)) void test_rm_false(void) {
  __asm__ volatile("mvi a, 0x01\n"
                   "ora a\n"
                   "rm\n"
                   "mvi a, 0x01\n"
                   "sta 0x05D5\n"
                   "ret\n");
}

__attribute__((naked)) void test_rpe_false(void) {
  __asm__ volatile("mvi a, 0x01\n"
                   "ora a\n"
                   "rpe\n"
                   "mvi a, 0x01\n"
                   "sta 0x05D6\n"
                   "ret\n");
}

__attribute__((naked)) void test_rpo_false(void) {
  __asm__ volatile("xra a\n"
                   "rpo\n"
                   "mvi a, 0x01\n"
                   "sta 0x05D7\n"
                   "ret\n");
}

__attribute__((naked)) int main(void) {
  __asm__ volatile("call test_rz_false\n"
                   "call test_rnz_false\n"
                   "call test_rc_false\n"
                   "call test_rnc_false\n"
                   "call test_rp_false\n"
                   "call test_rm_false\n"
                   "call test_rpe_false\n"
                   "call test_rpo_false\n"
                   "hlt\n");
}

// RFALSE: Memory dump 0x05D0 - 0x05D7 (8 bytes):
// RFALSE: 05D0: 01 01 01 01 01 01 01 01
// RFALSE: "halt":"hlt"
