; REQUIRES: i8085-sim
; RUN: %i8085_clang -target i8085 -c %i8085_sdk/sysroot/crt/crt0.S -o %t.crt.o
; RUN: %i8085_llc -mtriple=i8085 -O2 -verify-machineinstrs -filetype=obj %s -o %t.o
; RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o
; RUN: %i8085_objcopy -O binary %t.elf %t.bin
; RUN: %i8085_sim -S -q -n 100000 -d %testdump_addr:4 %t.bin 2>&1 | FileCheck %s
target datalayout = "e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8-a:8"
target triple = "i8085-unknown-elf"
@out = global [2 x i16] zeroinitializer, section ".testdump", align 1
@callee = global ptr @result, align 1
define i16 @result() nounwind { ret i16 4660 }
define i16 @indirect(ptr %p) noinline nounwind {
  %r = tail call i16 %p()
  ret i16 %r
}
define i16 @main() nounwind {
  %guard = alloca i16, align 1
  store volatile i16 -8531, ptr %guard, align 1
  %p = load volatile ptr, ptr @callee, align 1
  %r = call i16 @indirect(ptr %p)
  store volatile i16 %r, ptr @out, align 1
  %g = load volatile i16, ptr %guard, align 1
  store volatile i16 %g, ptr getelementptr ([2 x i16], ptr @out, i16 0, i16 1), align 1
  ret i16 0
}
; CHECK: FF00:{{ *}}34 12 AD DE
; CHECK: "halt":"hlt"
