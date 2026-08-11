; RUN: llc -mtriple=tms9900 -O0 -verify-machineinstrs < %s | FileCheck %s
;
; The C ABI guarantees 4-byte stack alignment. Objects with greater explicit
; alignment require a runtime-aligned frame. R13 anchors the incoming stack
; state and R14 addresses the aligned local frame.

declare void @consume(ptr)
declare void @consume5(i16, i16, i16, i16, ptr)

; CHECK-LABEL: aligned8_leaf:
; CHECK:      DECT R10
; CHECK-NEXT: MOV R13,*R10
; CHECK-NEXT: DECT R10
; CHECK-NEXT: MOV R14,*R10
; CHECK-NEXT: MOV R10,R13
; CHECK-NEXT: AI R10,-8
; CHECK-NEXT: ANDI R10,-8
; CHECK-NEXT: MOV R10,R14
; CHECK:      MOV R14,R0
; CHECK:      MOV R13,R10
; CHECK-NEXT: MOV *R10+,R14
; CHECK-NEXT: MOV *R10+,R13
; CHECK-NEXT: B *R11
define i16 @aligned8_leaf() noinline {
  %slot = alloca i16, align 8
  store volatile i16 1, ptr %slot, align 8
  %address = ptrtoint ptr %slot to i16
  %misalignment = and i16 %address, 7
  ret i16 %misalignment
}

; CHECK-LABEL: aligned16_nonleaf:
; CHECK:      DECT R10
; CHECK-NEXT: MOV R11,*R10
; CHECK-NEXT: DECT R10
; CHECK-NEXT: MOV R13,*R10
; CHECK-NEXT: DECT R10
; CHECK-NEXT: MOV R14,*R10
; CHECK-NEXT: MOV R10,R13
; CHECK:      ANDI R10,-16
; CHECK-NEXT: MOV R10,R14
; CHECK:      MOV R14,R0
; CHECK:      BL @consume
; CHECK:      MOV R13,R10
; CHECK-NEXT: MOV *R10+,R14
; CHECK-NEXT: MOV *R10+,R13
; CHECK-NEXT: MOV *R10+,R11
; CHECK-NEXT: B *R11
define i16 @aligned16_nonleaf() noinline {
  %slot = alloca i16, align 16
  store volatile i16 42, ptr %slot, align 16
  call void @consume(ptr %slot)
  %value = load volatile i16, ptr %slot, align 16
  ret i16 %value
}

; CHECK-LABEL: aligned8_stack_arg:
; CHECK:      MOV R10,R13
; CHECK:      ANDI R10,-8
; CHECK-NEXT: MOV R10,R14
; CHECK:      MOV R13,R0
; CHECK-NEXT: AI R0,6
; CHECK:      MOV R14,R0
; CHECK:      BL @consume
; CHECK:      MOV R13,R10
define i16 @aligned8_stack_arg(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e) noinline {
  %slot = alloca i16, align 8
  store volatile i16 %e, ptr %slot, align 8
  call void @consume(ptr %slot)
  %value = load volatile i16, ptr %slot, align 8
  ret i16 %value
}

; CHECK-LABEL: aligned16_vla:
; CHECK:      MOV R10,R13
; CHECK:      ANDI R10,-16
; CHECK-NEXT: MOV R10,R14
; CHECK:      MOV R10,R1
; CHECK-NEXT: S R0,R1
; CHECK-NEXT: MOV R1,R10
; CHECK:      MOV R14,R0
; CHECK:      MOV R13,R10
; CHECK-NEXT: MOV *R10+,R14
; CHECK-NEXT: MOV *R10+,R13
define i16 @aligned16_vla(i16 %count) noinline {
  %fixed = alloca i16, align 16
  %dynamic = alloca i8, i16 %count, align 2
  store volatile i16 99, ptr %fixed, align 16
  store volatile i8 7, ptr %dynamic, align 1
  %value = load volatile i16, ptr %fixed, align 16
  ret i16 %value
}

; CHECK-LABEL: aligned16_dynamic_only:
; CHECK:      MOV R10,R13
; CHECK-NEXT: ANDI R10,-16
; CHECK-NEXT: MOV R10,R14
; CHECK:      ANDI R{{[0-9]+}},-16
; CHECK:      MOV R13,R10
define i16 @aligned16_dynamic_only(i16 %count) noinline {
  %dynamic = alloca i8, i16 %count, align 16
  store volatile i8 1, ptr %dynamic, align 1
  %address = ptrtoint ptr %dynamic to i16
  %misalignment = and i16 %address, 15
  ret i16 %misalignment
}

; CHECK-LABEL: aligned16_outgoing_stack_arg:
; CHECK:      ANDI R10,-16
; CHECK-NEXT: MOV R10,R14
; CHECK:      MOV R14,R{{[0-9]+}}
; CHECK:      AI R10,-4
; CHECK-NEXT: MOV R10,R[[ARGADDR:[0-9]+]]
; CHECK-NEXT: MOV R{{[0-9]+}},*R[[ARGADDR]]
; CHECK:      BL @consume5
; CHECK:      AI R10,4
; CHECK:      MOV R13,R10
define i16 @aligned16_outgoing_stack_arg() noinline {
  %slot = alloca i16, align 16
  store volatile i16 77, ptr %slot, align 16
  call void @consume5(i16 1, i16 2, i16 3, i16 4, ptr %slot)
  %value = load volatile i16, ptr %slot, align 16
  ret i16 %value
}
