; REQUIRES: i8085-sim
; RUN: %i8085_llc -mtriple=i8085-unknown-elf -O2 -verify-machineinstrs %s -o - | \
; RUN:   FileCheck %s --check-prefix=ASM
; RUN: %i8085_clang -target i8085-unknown-elf -O2 -ffreestanding -fno-builtin -nostdlib -c \
; RUN:   %i8085_sdk/sysroot/crt/crt0.S -o %t.crt0.o
; RUN: %i8085_llc -mtriple=i8085-unknown-elf -O2 -verify-machineinstrs \
; RUN:   -filetype=obj %s -o %t.o
; RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld \
; RUN:   -o %t.elf %t.crt0.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
; RUN: %i8085_objcopy -O binary %t.elf %t.bin
; RUN: %i8085_sim -S -q -n 200000 -d %testdump_addr:0x08 %t.bin 2>&1 | \
; RUN:   FileCheck %s --check-prefix=SIM

target datalayout = "e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8-a:8"
target triple = "i8085-unknown-unknown-elf"

@out = global [4 x i16] zeroinitializer, section ".testdump", align 2
@v16_a = internal global i16 -25924, align 2 ; 0x9abc
@v16_b = internal global i16 5, align 2
@v16_c = internal global i16 3, align 2
@v8_a = internal global i8 7, align 1
@v8_b = internal global i8 3, align 1
@v8_neg = internal global i8 -127, align 1   ; 0x81
@v8_pos = internal global i8 127, align 1    ; 0x7f
@v8_sh3 = internal global i8 19, align 1     ; 0x13
@v8_m1 = internal global i8 19, align 1
@v8_m2 = internal global i8 11, align 1
@v8_m3 = internal global i8 9, align 1
@v32_a = internal global i32 16909060, align 4  ; 0x01020304
@v32_b = internal global i32 1122867, align 4   ; 0x00112233
@v32_c = internal global i32 65539, align 4     ; 0x00010003

declare void @llvm.memset.p0.i16(ptr nocapture writeonly, i8, i16, i1 immarg)
declare void @llvm.memcpy.p0.p0.i16(ptr nocapture writeonly, ptr nocapture readonly, i16, i1 immarg)

define internal fastcc i16 @shift16_ops() #0 {
entry:
  %a = load volatile i16, ptr @v16_a, align 2
  %b = load volatile i16, ptr @v16_b, align 2
  %c = load volatile i16, ptr @v16_c, align 2
  %s1 = lshr i16 %a, 15
  %s2 = lshr i16 %a, 13
  %s3 = shl i16 %b, 14
  %s4 = shl i16 %c, 15
  %x1 = xor i16 %s1, %s2
  %x2 = xor i16 %s3, %s4
  %res = xor i16 %x1, %x2
  ret i16 %res
}

define internal fastcc i16 @shift8_mul8_ops() #0 {
entry:
  %a = load volatile i8, ptr @v8_a, align 1
  %b = load volatile i8, ptr @v8_b, align 1
  %neg = load volatile i8, ptr @v8_neg, align 1
  %pos = load volatile i8, ptr @v8_pos, align 1
  %sh3_in = load volatile i8, ptr @v8_sh3, align 1
  %s1 = shl i8 %a, 6
  %s2 = shl i8 %b, 5
  %s3 = ashr i8 %neg, 7
  %s4 = ashr i8 %pos, 7
  %s5 = shl i8 %sh3_in, 3
  %t1 = xor i8 %s1, %s2
  %t2 = xor i8 %s3, %s4
  %t3 = xor i8 %t1, %t2
  %lo = xor i8 %t3, %s5

  %m1_in = load volatile i8, ptr @v8_m1, align 1
  %m2_in = load volatile i8, ptr @v8_m2, align 1
  %m3_in = load volatile i8, ptr @v8_m3, align 1
  %m1 = mul i8 %m1_in, 3
  %m2 = mul i8 %m2_in, 10
  %m3 = mul i8 %m3_in, 7
  %ms = add i8 %m1, %m2
  %hi = add i8 %ms, %m3

  %lo16 = zext i8 %lo to i16
  %hi16 = zext i8 %hi to i16
  %hish = shl i16 %hi16, 8
  %pair = or i16 %lo16, %hish
  ret i16 %pair
}

define internal fastcc i32 @mul32_small_memops() #0 {
entry:
  %buf = alloca [2 x i16], align 2
  call void @llvm.memset.p0.i16(ptr nonnull %buf, i8 0, i16 4, i1 false)
  %src = alloca i16, align 2
  store volatile i16 0, ptr %src, align 2
  call void @llvm.memcpy.p0.p0.i16(ptr nonnull %buf, ptr nonnull %src, i16 2, i1 false)
  %z = load volatile i16, ptr %buf, align 2
  %z32 = zext i16 %z to i32

  %a = load volatile i32, ptr @v32_a, align 4
  %b = load volatile i32, ptr @v32_b, align 4
  %c = load volatile i32, ptr @v32_c, align 4
  %ma = mul i32 %a, 5
  %mb = mul i32 %b, 257
  %mc = mul i32 %c, 7
  %x1 = xor i32 %ma, %mb
  %x2 = xor i32 %x1, %mc
  %res = add i32 %x2, %z32
  ret i32 %res
}

define i16 @main() #1 {
entry:
  %w0 = call fastcc i16 @shift16_ops()
  %w1 = call fastcc i16 @shift8_mul8_ops()
  %m32 = call fastcc i32 @mul32_small_memops()
  %w2 = trunc i32 %m32 to i16
  %hi = lshr i32 %m32, 16
  %w3 = trunc i32 %hi to i16
  store volatile i16 %w0, ptr @out, align 2
  store volatile i16 %w1, ptr getelementptr inbounds ([4 x i16], ptr @out, i16 0, i16 1), align 2
  store volatile i16 %w2, ptr getelementptr inbounds ([4 x i16], ptr @out, i16 0, i16 2), align 2
  store volatile i16 %w3, ptr getelementptr inbounds ([4 x i16], ptr @out, i16 0, i16 3), align 2
  ret i16 0
}

attributes #0 = { noinline nounwind }
attributes #1 = { nounwind }

; ASM-LABEL: shift16_ops:
; ASM: RLC
; ASM: ANI 1
; ASM: RRC
; ASM: ANI 192

; ASM-LABEL: shift8_mul8_ops:
; ASM-NOT: CALL __mul8
; ASM: RAL
; ASM-NEXT: SBB A
; ASM: ADD A
; ASM-NEXT: ADD A
; ASM-NEXT: ADD A
; ASM-NOT: CALL __mul8
; ASM: RET

; ASM-LABEL: mul32_small_memops:
; ASM-NOT: CALL memset
; ASM-NOT: CALL memcpy
; ASM-NOT: CALL __mul32
; ASM: RET

; SIM: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (8 bytes):
; SIM: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}05 C0 C7 E6 32 5A 3E 14
; SIM: "halt":"hlt"
