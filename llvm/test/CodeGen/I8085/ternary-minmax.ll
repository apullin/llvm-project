; RUN: llc -mattr=i8085,sram < %s -march=i8085 | FileCheck %s

; Ternary/conditional expression patterns: min, max, abs, clamp.
; Adapted from RISC-V select-const.ll and select-binop-identity.ll

; min(a, b) signed
define i16 @min_i16(i16 %a, i16 %b) {
; CHECK-LABEL: min_i16:
; CHECK: XRA D
; CHECK: ANI 128
; CHECK: SBB
; CHECK: RET
entry:
  %cmp = icmp slt i16 %a, %b
  %r = select i1 %cmp, i16 %a, i16 %b
  ret i16 %r
}

; max(a, b) signed
define i16 @max_i16(i16 %a, i16 %b) {
; CHECK-LABEL: max_i16:
; CHECK: XRA D
; CHECK: ANI 128
; CHECK: SBB
; CHECK: RET
entry:
  %cmp = icmp sgt i16 %a, %b
  %r = select i1 %cmp, i16 %a, i16 %b
  ret i16 %r
}

; abs(a) = (a < 0) ? -a : a
define i16 @abs_i16(i16 %a) {
; CHECK-LABEL: abs_i16:
; CHECK: XRA D
; CHECK: ANI 128
; CHECK: SUB C
; CHECK: SBB B
; CHECK: RET
entry:
  %cmp = icmp slt i16 %a, 0
  %neg = sub i16 0, %a
  %r = select i1 %cmp, i16 %neg, i16 %a
  ret i16 %r
}

; Conditional add: (cond ? a+b : a)
define i16 @cond_add(i16 %a, i16 %b, i1 %cond) {
; CHECK-LABEL: cond_add:
; CHECK: ANI 1
; CHECK: JZ
; CHECK: ADD E
; CHECK: ADC D
; CHECK: RET
entry:
  %sum = add i16 %a, %b
  %r = select i1 %cond, i16 %sum, i16 %a
  ret i16 %r
}

; Unsigned min for i8
define i8 @umin_i8(i8 %a, i8 %b) {
; CHECK-LABEL: umin_i8:
; CHECK: SUB C
; CHECK: JC
; CHECK: RET
entry:
  %cmp = icmp ult i8 %a, %b
  %r = select i1 %cmp, i8 %a, i8 %b
  ret i8 %r
}

; clamp(x, lo, hi) = min(max(x, lo), hi)
define i16 @clamp_i16(i16 %x, i16 %lo, i16 %hi) {
; CHECK-LABEL: clamp_i16:
; CHECK: XRA D
; CHECK: ANI 128
; CHECK: RET
entry:
  %cmp1 = icmp slt i16 %x, %lo
  %mid = select i1 %cmp1, i16 %lo, i16 %x
  %cmp2 = icmp sgt i16 %mid, %hi
  %r = select i1 %cmp2, i16 %hi, i16 %mid
  ret i16 %r
}
