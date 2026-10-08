; REQUIRES: i8085-sim
; RUN: %i8085_clang -target i8085 -c %i8085_sdk/sysroot/crt/crt0.S -o %t.crt.o
; RUN: %i8085_llc -mtriple=i8085 -O0 -verify-machineinstrs -filetype=obj %s -o %t.o
; RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
; RUN: %i8085_objcopy -O binary %t.elf %t.bin
; RUN: %i8085_sim -S -q -n 1000000 -d %testdump_addr:4 %t.bin 2>&1 | FileCheck %s
; RUN: %i8085_llc -mtriple=i8085 -O2 -verify-machineinstrs -filetype=obj %s -o %t.o
; RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
; RUN: %i8085_objcopy -O binary %t.elf %t.bin
; RUN: %i8085_sim -S -q -n 1000000 -d %testdump_addr:4 %t.bin 2>&1 | FileCheck %s

target datalayout = "e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8-a:8"
target triple = "i8085-unknown-elf"
@out = global [2 x i16] zeroinitializer, section ".testdump", align 1
@bytes = global [17 x i8] c"\11\22\33\44\55\66\77\88\11\22\33\44\55\66\77\88\99", align 1

define i16 @read_bytes(i8 %head, ptr byval([17 x i8]) align 1 %p, i16 %tail) noinline nounwind {
  %first = load volatile i8, ptr %p, align 1
  %lastp = getelementptr i8, ptr %p, i16 16
  %last = load volatile i8, ptr %lastp, align 1
  store volatile i8 0, ptr %p, align 1
  %a = zext i8 %first to i16
  %b = zext i8 %last to i16
  %h = zext i8 %head to i16
  %r0 = add i16 %a, %b
  %r1 = add i16 %r0, %h
  %r = add i16 %r1, %tail
  ret i16 %r
}

define i16 @main() nounwind {
  %r = call i16 @read_bytes(i8 5, ptr byval([17 x i8]) align 1 @bytes, i16 4660)
  store volatile i16 %r, ptr @out, align 1
  %v = load volatile i8, ptr @bytes, align 1
  %orig = zext i8 %v to i16
  store volatile i16 %orig, ptr getelementptr ([2 x i16], ptr @out, i16 0, i16 1), align 1
  ret i16 0
}
; CHECK: FF00:{{ *}}E3 12 11 00
; CHECK: "halt":"hlt"
