; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=tms9900 %t/overaligned.ll -o /dev/null 2>&1 | FileCheck %s --check-prefix=ALIGN
; RUN: not --crash llc -mtriple=tms9900 %t/oversized.ll -o /dev/null 2>&1 | FileCheck %s --check-prefix=SIZE

; ALIGN: LLVM ERROR: TMS9900: stack alignment must be less than 65536 bytes
; SIZE: LLVM ERROR: TMS9900: stack frame exceeds the 16-bit address space

;--- overaligned.ll
define ptr @overaligned() {
  %slot = alloca i8, align 65536
  ret ptr %slot
}

;--- oversized.ll
define void @oversized() {
  %slot = alloca [65536 x i8], align 4
  %last = getelementptr [65536 x i8], ptr %slot, i16 0, i16 -1
  store volatile i8 1, ptr %last
  ret void
}
