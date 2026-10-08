; RUN: llc -mtriple=tms9900 -O0 -verify-machineinstrs %s -o - | FileCheck %s
; RUN: llc -mtriple=tms9900 -O2 -verify-machineinstrs %s -o - | FileCheck %s

; The stack boundary is four bytes even inside a leaf function whose locals
; have only byte or word alignment. The transient stack alignment must not
; allow PEI to allocate a one- or two-byte frame.

define i16 @leaf_word(i16 %value) {
; CHECK-LABEL: leaf_word:
; CHECK: AI R10,-4
; CHECK: AI R10,4
; CHECK: B *R11
  %slot = alloca i16, align 2
  store volatile i16 %value, ptr %slot, align 2
  %loaded = load volatile i16, ptr %slot, align 2
  ret i16 %loaded
}

define i8 @leaf_byte(i8 %value) {
; CHECK-LABEL: leaf_byte:
; CHECK: AI R10,-4
; CHECK: AI R10,4
; CHECK: B *R11
  %slot = alloca i8, align 1
  store volatile i8 %value, ptr %slot, align 1
  %loaded = load volatile i8, ptr %slot, align 1
  ret i8 %loaded
}
