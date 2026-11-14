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
; RUN: %i8085_sim -S -q -n 500000 --irq=65@400 --irq=65@900 --irq=65@1400 --irq=65@1900 --irq=65@2400 --irq=65@2900 --irq=65@3400 --irq=65@3900 -d %testdump_addr:0x06 %t.bin 2>&1 | \
; RUN:   FileCheck %s --check-prefix=SIM

target datalayout = "e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8-a:8"
target triple = "i8085-unknown-unknown-elf"

@out = global [3 x i16] zeroinitializer, section ".testdump", align 2
@irq_ticks = internal global i16 0, align 2
@irq_mix = internal global i32 305419896, align 4

; Helper called from inside the ISR to clobber A, BC, DE, HL, and flags.
define internal fastcc i16 @isr_clobber_helper(i16 %x) #0 {
entry:
  %m = load volatile i32, ptr @irq_mix, align 4
  %ext = zext i16 %x to i32
  %add = add i32 %m, %ext
  %xor = xor i32 %add, -1640531527
  store volatile i32 %xor, ptr @irq_mix, align 4
  %tr = trunc i32 %xor to i16
  ret i16 %tr
}

; Maskable RST 6.5 handler ("interrupt"): saves PSW/BC/DE/HL, calls a helper,
; restores all registers + flags, and executes EI; RET.
define void @isr_rst65() #1 {
entry:
  %t = load volatile i16, ptr @irq_ticks, align 2
  %inc = add i16 %t, 1
  store volatile i16 %inc, ptr @irq_ticks, align 2
  %junk = call fastcc i16 @isr_clobber_helper(i16 %inc)
  ret void
}

; Non-re-enabling handler ("signal"): saves/restores PSW/BC/DE/HL, plain RET.
define void @isr_trap() #2 {
entry:
  %t = load volatile i16, ptr @irq_ticks, align 2
  %inc = add i16 %t, 16
  store volatile i16 %inc, ptr @irq_ticks, align 2
  ret void
}

define i16 @main() #3 {
entry:
  ; Unmask RST 6.5 (MSE=1, M7.5=1, M6.5=0, M5.5=1 -> 0x0D) and enable interrupts.
  call void asm sideeffect "MVI A, 13\0ASIM\0AEI", "~{a}"()
  br label %loop

loop:
  %i = phi i16 [ 0, %entry ], [ %next_i, %loop ]
  %acc = phi i32 [ 305419896, %entry ], [ %next_acc, %loop ]
  %ext = zext i16 %i to i32
  %shl = shl i32 %acc, 3
  %shr = lshr i32 %acc, 2
  %mix1 = xor i32 %shl, %shr
  %mix2 = add i32 %mix1, %ext
  %next_acc = add i32 %mix2, 40503
  %next_i = add i16 %i, 1
  %done = icmp eq i16 %next_i, 128
  br i1 %done, label %exit, label %loop

exit:
  call void asm sideeffect "DI", ""()
  %lo = trunc i32 %next_acc to i16
  %hi32 = lshr i32 %next_acc, 16
  %hi = trunc i32 %hi32 to i16
  %ticks = load volatile i16, ptr @irq_ticks, align 2
  %fired_many = icmp uge i16 %ticks, 8
  %flag = zext i1 %fired_many to i16
  store volatile i16 %lo, ptr @out, align 2
  store volatile i16 %hi, ptr getelementptr inbounds ([3 x i16], ptr @out, i16 0, i16 1), align 2
  store volatile i16 %flag, ptr getelementptr inbounds ([3 x i16], ptr @out, i16 0, i16 2), align 2
  ret i16 0
}

attributes #0 = { noinline nounwind }
attributes #1 = { nounwind "interrupt" }
attributes #2 = { nounwind "signal" }
attributes #3 = { nounwind }

; ASM-LABEL: isr_rst65:
; ASM: PUSH PSW
; ASM: PUSH B
; ASM: PUSH D
; ASM: PUSH H
; ASM: CALL isr_clobber_helper
; ASM: POP H
; ASM: POP D
; ASM: POP B
; ASM: POP PSW
; ASM-NEXT: EI
; ASM-NEXT: RET

; ASM-LABEL: isr_trap:
; ASM: PUSH PSW
; ASM: PUSH B
; ASM: PUSH D
; ASM: PUSH H
; ASM: POP H
; ASM: POP D
; ASM: POP B
; ASM: POP PSW
; ASM-NOT: EI
; ASM-NEXT: RET

; SIM: Memory dump {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} - {{0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}} (6 bytes):
; SIM: {{ *}}{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}:{{ *}}FE ED 18 AC 01 00
; SIM: "halt":"hlt"
