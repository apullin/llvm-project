; REQUIRES: i8085-sim
; RUN: %i8085_llc -mtriple=i8085-unknown-elf -O2 -i8085-scratchpad \
; RUN:   -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=ASM
; RUN: %i8085_clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -c \
; RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
; RUN: %i8085_llc -mtriple=i8085-unknown-elf -O2 -i8085-scratchpad \
; RUN:   -verify-machineinstrs -filetype=obj %s -o %t.o
; RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
; RUN:   -o %t.elf %t.crt0.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
; RUN: %i8085_objcopy -O binary %t.elf %t.bin
; RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x06 %t.bin 2>&1 | \
; RUN:   FileCheck %s --check-prefix=SIM

target datalayout = "e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8-a:8"
target triple = "i8085-unknown-unknown-elf"

@out = global [3 x i16] zeroinitializer, section ".testdump", align 2
@seed = internal global i16 4951, align 2 ; 0x1357
@depth = internal global i16 12, align 2

define internal fastcc i32 @leaf(i32 %x) #0 {
entry:
  %m = mul i32 %x, 3
  %a = add i32 %m, 66051 ; 0x10203
  ret i32 %a
}

; Closed acyclic call cone depth = 1 -> call-live spills use @__i8085_vreg_overlay_d1
define internal fastcc i32 @mid_a(i16 %x) #0 {
entry:
  %t = xor i16 %x, 21930 ; 0x55aa
  %x32 = zext i16 %x to i32
  %arg = add i32 %x32, 7
  %lv = call fastcc i32 @leaf(i32 %arg)
  %t32 = zext i16 %t to i32
  %sh = shl i16 %x, 1
  %sh32 = zext i16 %sh to i32
  %s1 = add i32 %lv, %t32
  %s2 = add i32 %s1, %sh32
  ret i32 %s2
}

; Sibling closed acyclic call cone depth = 1 -> shares @__i8085_vreg_overlay_d1 safely
define internal fastcc i32 @mid_b(i16 %x) #0 {
entry:
  %t = add i16 %x, 4660 ; 0x1234
  %x32 = zext i16 %x to i32
  %arg = xor i32 %x32, 48879 ; 0xbeef
  %lv = call fastcc i32 @leaf(i32 %arg)
  %t32 = zext i16 %t to i32
  %s1 = xor i32 %lv, %t32
  %s2 = xor i32 %s1, %x32
  ret i32 %s2
}

; Closed acyclic call cone depth = 2 -> call-live spills use @__i8085_vreg_overlay_d2
; (provably disjoint from mid_a/mid_b's @__i8085_vreg_overlay_d1!)
define internal fastcc i32 @root_tree(i16 %x) #0 {
entry:
  %a = call fastcc i32 @mid_a(i16 %x)
  %xp9 = add i16 %x, 9
  %b = call fastcc i32 @mid_b(i16 %xp9)
  %x32 = zext i16 %x to i32
  %s1 = add i32 %a, %b
  %s2 = add i32 %s1, %x32
  ret i32 %s2
}

; Recursive function in the same module: closed-cone analysis detects the cycle
; and keeps call-live state on the stack so recursion remains exact.
define internal fastcc i16 @rec_sum(i16 %n) #0 {
entry:
  %z = icmp eq i16 %n, 0
  br i1 %z, label %done, label %rec

rec:
  %nm1 = sub i16 %n, 1
  %sub = call fastcc i16 @rec_sum(i16 %nm1)
  %x = xor i16 %sub, 3
  %r = add i16 %n, %x
  ret i16 %r

done:
  ret i16 0
}

define i16 @main() #1 {
entry:
  %s = load volatile i16, ptr @seed, align 2
  %d = load volatile i16, ptr @depth, align 2
  %v = call fastcc i32 @root_tree(i16 %s)
  %r = call fastcc i16 @rec_sum(i16 %d)
  %lo = trunc i32 %v to i16
  %hi32 = lshr i32 %v, 16
  %hi = trunc i32 %hi32 to i16
  store volatile i16 %lo, ptr @out, align 2
  store volatile i16 %hi, ptr getelementptr inbounds ([3 x i16], ptr @out, i16 0, i16 1), align 2
  store volatile i16 %r, ptr getelementptr inbounds ([3 x i16], ptr @out, i16 0, i16 2), align 2
  ret i16 0
}

attributes #0 = { noinline nounwind }
attributes #1 = { nounwind }

; ASM: __i8085_vreg_gr32
; ASM: __i8085_vreg_overlay_d1
; ASM: __i8085_vreg_overlay_d2

; SIM: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (6 bytes):
; SIM: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}63 F9 04 00 56 00
; SIM: "halt":"hlt"
