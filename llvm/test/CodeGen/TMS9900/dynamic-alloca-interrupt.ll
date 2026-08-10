; RUN: not --crash llc -mtriple=tms9900 -O2 < %s 2>&1 | FileCheck %s
;
; R13-R15 contain the hardware return context in an interrupt workspace, so
; R13 cannot also serve as the stable frame pointer required by a VLA.
;
; CHECK: LLVM ERROR: TMS9900: variable stack objects and frame addresses are unsupported in naked and interrupt functions

define void @interrupt_vla(i16 %count) #0 {
  %storage = alloca i16, i16 %count, align 2
  store volatile i16 1, ptr %storage, align 2
  ret void
}

attributes #0 = { "interrupt" }
