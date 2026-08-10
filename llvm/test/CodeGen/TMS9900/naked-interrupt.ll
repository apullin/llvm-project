; RUN: llc -mtriple=tms9900 -O2 < %s | FileCheck %s
;
; Test function attributes: naked and interrupt.
;
; Naked functions should have no prologue/epilogue.
; Interrupt functions should use RTWP (return from interrupt) instead of B *R11.

; --- Naked function: no prologue, no epilogue ---
; CHECK-LABEL: naked_func:
; CHECK-NOT: DECT{{[ \t]+}}R10
; CHECK-NOT: MOV{{[ \t]+}}R11
; CHECK: LI{{[ \t]+}}R0,42
; CHECK: B{{[ \t]+}}*R11
; CHECK-NOT: MOV{{[ \t]+}}*R10+,R11

define void @naked_func() naked {
  call void asm sideeffect "LI R0, 42\0AB *R11", ""()
  unreachable
}

; --- Interrupt handler: no prologue, returns with RTWP ---
; CHECK-LABEL: isr:
; CHECK-NOT: DECT{{[ \t]+}}R10
; CHECK-NOT: B{{[ \t]+}}*R11
; CHECK: RTWP

define void @isr() #0 {
  ret void
}

attributes #0 = { "interrupt" }

; --- Interrupt register pressure: preserve the hardware RTWP context ---
; Interrupt entry places the old WP, PC, and ST in R13, R14, and R15.
; Holding twelve volatile values live across a call forces spills after those
; registers are reserved.  The handler must allocate local stack space without
; saving R11, because RTWP does not consume the link register.
;
; CHECK-LABEL: isr_pressure:
; CHECK-NOT: R13
; CHECK-NOT: R14
; CHECK-NOT: R15
; CHECK-NOT: DECT{{[ \t]+}}R10
; CHECK-NOT: MOV{{.*}}R11
; CHECK: AI{{[ \t]+}}R10,-[[FRAME:[0-9]+]]
; CHECK-NOT: R13
; CHECK-NOT: R14
; CHECK-NOT: R15
; CHECK-NOT: MOV{{.*}}R11
; CHECK: BL{{[ \t]+}}@interrupt_helper
; CHECK-NOT: R13
; CHECK-NOT: R14
; CHECK-NOT: R15
; CHECK-NOT: MOV{{.*}}R11
; CHECK: AI{{[ \t]+}}R10,[[FRAME]]
; CHECK-NOT: R13
; CHECK-NOT: R14
; CHECK-NOT: R15
; CHECK-NOT: MOV{{.*}}R11
; CHECK-NEXT: RTWP

@g0 = external global i16
@g1 = external global i16
@g2 = external global i16
@g3 = external global i16
@g4 = external global i16
@g5 = external global i16
@g6 = external global i16
@g7 = external global i16
@g8 = external global i16
@g9 = external global i16
@g10 = external global i16
@g11 = external global i16

declare void @interrupt_helper()

define void @isr_pressure() #0 {
  %v0 = load volatile i16, ptr @g0
  %v1 = load volatile i16, ptr @g1
  %v2 = load volatile i16, ptr @g2
  %v3 = load volatile i16, ptr @g3
  %v4 = load volatile i16, ptr @g4
  %v5 = load volatile i16, ptr @g5
  %v6 = load volatile i16, ptr @g6
  %v7 = load volatile i16, ptr @g7
  %v8 = load volatile i16, ptr @g8
  %v9 = load volatile i16, ptr @g9
  %v10 = load volatile i16, ptr @g10
  %v11 = load volatile i16, ptr @g11
  call void @interrupt_helper()
  store volatile i16 %v0, ptr @g0
  store volatile i16 %v1, ptr @g1
  store volatile i16 %v2, ptr @g2
  store volatile i16 %v3, ptr @g3
  store volatile i16 %v4, ptr @g4
  store volatile i16 %v5, ptr @g5
  store volatile i16 %v6, ptr @g6
  store volatile i16 %v7, ptr @g7
  store volatile i16 %v8, ptr @g8
  store volatile i16 %v9, ptr @g9
  store volatile i16 %v10, ptr @g10
  store volatile i16 %v11, ptr @g11
  ret void
}
