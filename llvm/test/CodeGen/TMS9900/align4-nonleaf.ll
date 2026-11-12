; RUN: llc -mtriple=tms9900 -O2 -verify-machineinstrs < %s | FileCheck %s

declare void @sink(ptr)

; Non-leaf functions push 2-byte R11 and allocate StackSize + 2 so R10 stays
; 4-byte aligned. A 4-byte-aligned local object must sit at a multiple of 4
; from R10 (e.g. *R10 or @4(R10)), never at +2.
define i16 @nonleaf_align4_local() {
; CHECK-LABEL: nonleaf_align4_local:
; CHECK:       DECT R10
; CHECK-NEXT:  MOV R11,*R10
; CHECK-NEXT:  AI R10,-6
; CHECK:       MOV R10,R0
; CHECK-NOT:   INCT R0
; CHECK-NOT:   AI R0,2
; CHECK:       BL @sink
  %x = alloca i16, align 4
  store volatile i16 4660, ptr %x, align 4
  call void @sink(ptr nonnull %x)
  ret i16 0
}
