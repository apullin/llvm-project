; RUN: not --crash llc -mtriple=tms9900 -O0 < %s 2>&1 | FileCheck %s
;
; R13 and R14 hold hardware return context in an interrupt workspace, so they
; cannot serve as the restore anchor and aligned base for a realigned frame.
;
; CHECK: LLVM ERROR: TMS9900: stack realignment is unsupported in naked and interrupt functions

define void @interrupt_aligned() #0 {
  %slot = alloca i16, align 8
  store volatile i16 1, ptr %slot, align 8
  ret void
}

attributes #0 = { "interrupt" }
