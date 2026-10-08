; REQUIRES: i8085-sim
; RUN: %i8085_clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -c \
; RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
; RUN: %i8085_llc -mtriple=i8085-unknown-elf -mattr=i8085,sram -O2 \
; RUN:   -verify-machineinstrs \
; RUN:   -filetype=obj %s -o %t.o
; RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
; RUN:   -o %t.elf %t.crt0.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
; RUN: %i8085_objcopy -O binary %t.elf %t.bin
; RUN: %i8085_sim -S -q -n 500000 -d %testdump_addr:0x04 %t.bin 2>&1 | \
; RUN:   FileCheck %s

target datalayout = "e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8-a:8"
target triple = "i8085-unknown-unknown-elf"

@out = global [2 x i16] zeroinitializer, section ".testdump", align 2
@element_count = internal global i16 4, align 1
@escaped = internal global ptr null, align 1

define internal i32 @exercise_vla_realign() #0 {
entry:
  %count = load volatile i16, ptr @element_count, align 1
  %values = alloca i16, i16 %count, align 4
  store volatile ptr %values, ptr @escaped, align 1
  store volatile i16 22136, ptr %values, align 1
  %value = load volatile i16, ptr %values, align 1
  %wide = zext i16 %value to i32
  %result = or i32 %wide, -1526726656
  ret i32 %result
}

define i16 @main() #1 {
entry:
  %result = call i32 @exercise_vla_realign()
  %low = trunc i32 %result to i16
  store volatile i16 %low, ptr @out, align 2
  %shift = lshr i32 %result, 16
  %high = trunc i32 %shift to i16
  store volatile i16 %high,
      ptr getelementptr inbounds ([2 x i16], ptr @out, i16 0, i16 1), align 2
  ret i16 0
}

attributes #0 = { noinline nounwind "frame-pointer"="all" "stackrealign" }
attributes #1 = { nounwind }

; CHECK: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (4 bytes):
; CHECK: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}78 56 00 A5
; CHECK: "halt":"hlt"
