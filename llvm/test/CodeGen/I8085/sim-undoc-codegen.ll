; REQUIRES: i8085-sim
; RUN: %i8085_llc -mtriple=i8085-unknown-elf -mcpu=i8085-undoc -O2 \
; RUN:   -enable-machine-outliner=always -verify-machineinstrs %s -o - | \
; RUN:   FileCheck %s --check-prefix=ASM
; RUN: %i8085_clang -target i8085-unknown-elf -mcpu=i8085-undoc -Oz -ffreestanding -fno-builtin -nostdlib -c \
; RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
; RUN: %i8085_llc -mtriple=i8085-unknown-elf -mcpu=i8085-undoc -O2 \
; RUN:   -enable-machine-outliner=always -verify-machineinstrs -filetype=obj %s -o %t.o
; RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
; RUN:   -o %t.elf %t.crt0.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
; RUN: %i8085_objcopy -O binary %t.elf %t.bin
; RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x04 %t.bin 2>&1 | \
; RUN:   FileCheck %s --check-prefix=SIM

target datalayout = "e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8-a:8"
target triple = "i8085-unknown-unknown-elf"

@out = global [2 x i16] zeroinitializer, section ".testdump", align 2
@p1 = internal global i16 4951, align 2   ; 0x1357
@p2 = internal global i16 9320, align 2   ; 0x2468

define i16 @undoc_fn1(i16 %x, i16 %y, ptr %p) #0 {
entry:
  %v = load i16, ptr %p, align 2
  %d = sub i16 %x, %y
  %s = ashr i16 %d, 1
  %l = shl i16 %y, 3
  %x1 = xor i16 %v, %d
  %x2 = xor i16 %x1, %s
  %x3 = xor i16 %x2, %l
  %res = add i16 %x3, 1
  ret i16 %res
}

define i16 @undoc_fn2(i16 %x, i16 %y, ptr %p) #0 {
entry:
  %v = load i16, ptr %p, align 2
  %d = sub i16 %x, %y
  %s = ashr i16 %d, 1
  %l = shl i16 %y, 3
  %x1 = xor i16 %v, %d
  %x2 = xor i16 %x1, %s
  %x3 = xor i16 %x2, %l
  %res = add i16 %x3, 2
  ret i16 %res
}

define i16 @main() #1 {
entry:
  %r1 = call i16 @undoc_fn1(i16 4660, i16 9029, ptr @p1)
  %r2 = call i16 @undoc_fn2(i16 -28672, i16 4369, ptr @p2)
  store volatile i16 %r1, ptr @out, align 2
  store volatile i16 %r2, ptr getelementptr inbounds ([2 x i16], ptr @out, i16 0, i16 1), align 2
  ret i16 0
}

attributes #0 = { minsize noinline nounwind optsize }
attributes #1 = { minsize nounwind optsize }

; ASM: CALL OUTLINED_FUNCTION_
; ASM: LDSI
; ASM: LHLX
; ASM: ARHL

; SIM: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (4 bytes):
; SIM: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}E8 10 7A ED
; SIM: "halt":"hlt"
