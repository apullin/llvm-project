; REQUIRES: i8085-sim
; RUN: %i8085_llc -mtriple=i8085 -O2 -stop-after=finalize-isel %s -o - | FileCheck %s --check-prefix=ISEL
; RUN: %i8085_clang -target i8085-unknown-elf -c %i8085_sdk/sysroot/crt/crt0.S -o %t.crt.o
; RUN: %i8085_llc -mtriple=i8085 -O0 -verify-machineinstrs -filetype=obj %s -o %t.o
; RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
; RUN: %i8085_objcopy -O binary %t.elf %t.bin
; RUN: %i8085_sim -S -q -n 100000 -d %testdump_addr:8 %t.bin 2>&1 | FileCheck %s
; RUN: %i8085_llc -mtriple=i8085 -O2 -verify-machineinstrs -filetype=obj %s -o %t.o
; RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
; RUN: %i8085_objcopy -O binary %t.elf %t.bin
; RUN: %i8085_sim -S -q -n 100000 -d %testdump_addr:8 %t.bin 2>&1 | FileCheck %s

target triple = "i8085-unknown-elf"
@word = global i16 13330, align 1
@words = global [2 x i16] [i16 8721, i16 17459], align 1
@wide = global i32 2018915346, align 1
@out = global [8 x i8] zeroinitializer, section ".testdump", align 1

define i8 @high_byte() noinline {
  %v = load i16, ptr @word, align 1
  %shift = lshr i16 %v, 8
  %r = trunc i16 %shift to i8
  ret i8 %r
}
; ISEL-LABEL: name: high_byte
; ISEL: LOAD_8_WITH_IMM_ADDR @word + 1

define i8 @nested_addend() noinline {
  %v = load i16, ptr getelementptr ([2 x i16], ptr @words, i16 0, i16 1), align 1
  %shift = lshr i16 %v, 8
  %r = trunc i16 %shift to i8
  ret i8 %r
}
; ISEL-LABEL: name: nested_addend
; ISEL: LOAD_8_WITH_IMM_ADDR @words + 3

define i16 @high_word() noinline {
  %v = load i32, ptr @wide, align 1
  %shift = lshr i32 %v, 16
  %r = trunc i32 %shift to i16
  ret i16 %r
}
; ISEL-LABEL: name: high_word
; ISEL: LOAD_16_WITH_IMM_ADDR @wide + 2

define i16 @main() {
  %a = call i8 @high_byte()
  store volatile i8 %a, ptr @out
  %b = call i8 @nested_addend()
  store volatile i8 %b, ptr getelementptr ([8 x i8], ptr @out, i16 0, i16 1)
  %c = call i16 @high_word()
  store volatile i16 %c, ptr getelementptr ([8 x i8], ptr @out, i16 0, i16 2), align 1
  store volatile i8 85, ptr getelementptr ([8 x i8], ptr @out, i16 0, i16 4)
  store volatile i8 170, ptr getelementptr ([8 x i8], ptr @out, i16 0, i16 5)
  ret i16 0
}
; CHECK: FF00:{{ *}}34 44 56 78 55 AA 00 00
; CHECK: "halt":"hlt"
