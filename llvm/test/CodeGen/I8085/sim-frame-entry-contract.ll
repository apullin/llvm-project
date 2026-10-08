; REQUIRES: i8085-sim
; RUN: %i8085_clang -target i8085-unknown-elf -c %i8085_sdk/sysroot/crt/crt0.S -o %t.crt.o
; RUN: %i8085_llc -mtriple=i8085 -O0 -verify-machineinstrs -filetype=obj %s -o %t.o
; RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
; RUN: %i8085_objcopy -O binary %t.elf %t.bin
; RUN: %i8085_sim -S -q -n 1000000 -d %testdump_addr:16 %t.bin 2>&1 | FileCheck %s
; RUN: %i8085_llc -mtriple=i8085 -O2 -verify-machineinstrs -filetype=obj %s -o %t.o
; RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
; RUN: %i8085_objcopy -O binary %t.elf %t.bin
; RUN: %i8085_sim -S -q -n 1000000 -d %testdump_addr:16 %t.bin 2>&1 | FileCheck %s

target datalayout = "e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8-a:8"
target triple = "i8085-unknown-elf"
@out = global [8 x i16] zeroinitializer, section ".testdump", align 1
@wide = global i32 287454020, align 1
@count = global i16 4, align 1
@escaped = global ptr null, align 1

define i32 @omit_frame() #0 {
  %v = load volatile i32, ptr @wide, align 1
  %r = add i32 %v, 1
  ret i32 %r
}

define i16 @aligned_c(i8 %first, i16 %second) #1 {
  %a = alloca [16 x i8], align 16
  store volatile ptr %a, ptr @escaped, align 1
  store volatile i8 99, ptr %a, align 1
  %x = zext i8 %first to i16
  %r = add i16 %x, %second
  ret i16 %r
}

define fastcc i16 @dynamic_fast(i16 %x, i16 %y) #2 {
  %n = load volatile i16, ptr @count, align 1
  %a = alloca i16, i16 %n, align 1
  store volatile ptr %a, ptr @escaped, align 1
  store volatile i16 %x, ptr %a, align 1
  %v = load volatile i16, ptr %a, align 1
  %r = add i16 %v, %y
  ret i16 %r
}

define fastcc i16 @aligned_fast(i16 %x, i16 %y) #1 {
  %a = alloca [16 x i8], align 16
  store volatile ptr %a, ptr @escaped, align 1
  store volatile i16 %x, ptr %a, align 1
  %v = load volatile i16, ptr %a, align 1
  %r = add i16 %v, %y
  ret i16 %r
}

define i16 @main() #2 {
  %canary = alloca i16, align 1
  store volatile i16 -8531, ptr %canary, align 1
  %w = call i32 @omit_frame()
  %lo = trunc i32 %w to i16
  store volatile i16 %lo, ptr @out, align 1
  %wh = lshr i32 %w, 16
  %hi = trunc i32 %wh to i16
  store volatile i16 %hi, ptr getelementptr ([7 x i16], ptr @out, i16 0, i16 1), align 1
  %c = call i16 @aligned_c(i8 7, i16 4660)
  store volatile i16 %c, ptr getelementptr ([7 x i16], ptr @out, i16 0, i16 2), align 1
  %d = call fastcc i16 @dynamic_fast(i16 4660, i16 22136)
  store volatile i16 %d, ptr getelementptr ([7 x i16], ptr @out, i16 0, i16 3), align 1
  %f = call fastcc i16 @aligned_fast(i16 4660, i16 22136)
  store volatile i16 %f, ptr getelementptr ([7 x i16], ptr @out, i16 0, i16 4), align 1
  %guard = load volatile i16, ptr %canary, align 1
  store volatile i16 %guard, ptr getelementptr ([7 x i16], ptr @out, i16 0, i16 5), align 1
  store volatile i16 1, ptr getelementptr ([8 x i16], ptr @out, i16 0, i16 6), align 1
  %last = load volatile ptr, ptr @escaped, align 1
  %addr = ptrtoint ptr %last to i16
  %misalign = and i16 %addr, 15
  store volatile i16 %misalign, ptr getelementptr ([8 x i16], ptr @out, i16 0, i16 7), align 1
  ret i16 0
}

attributes #0 = { noinline nounwind "frame-pointer"="none" }
; Over-aligned locals require realignment without an explicit attribute.
attributes #1 = { noinline nounwind }
attributes #2 = { noinline nounwind }
; CHECK: FF00:{{ *}}45 33 22 11 3B 12 AC 68 AC 68 AD DE 01 00 00 00
; CHECK: "halt":"hlt"
