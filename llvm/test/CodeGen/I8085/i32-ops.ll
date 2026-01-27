; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Exercise 32-bit pseudos and verify they are expanded before asm emission.

define i32 @i32_arith(i32 %a, i32 %b) {
; CHECK-LABEL: i32_arith:
; CHECK-NOT: ADD_32
; CHECK-NOT: SUB_32
; CHECK-NOT: AND_32
; CHECK-NOT: OR_32
; CHECK-NOT: XOR_32
; CHECK-NOT: MVI_32
; CHECK: RET
entry:
  %add = add i32 %a, %b
  %sub = sub i32 %add, 42
  %and = and i32 %sub, 305419896
  %or = or i32 %and, %b
  %xor = xor i32 %or, -1
  ret i32 %xor
}

define i32 @i32_shift(i32 %a, i8 %sh) {
; CHECK-LABEL: i32_shift:
; CHECK-NOT: SHL_32
; CHECK-NOT: SRA_32
; CHECK-NOT: RL_32
; CHECK-NOT: RR_32
; CHECK: RET
entry:
  %sh32 = zext i8 %sh to i32
  %shl = shl i32 %a, %sh32
  %shr = ashr i32 %shl, %sh32
  ret i32 %shr
}

define i8 @i32_cmp(i32 %a, i32 %b) {
; CHECK-LABEL: i32_cmp:
; CHECK-NOT: SET_
; CHECK: RET
entry:
  %cmp = icmp slt i32 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

define i32 @i32_mem() {
; CHECK-LABEL: i32_mem:
; CHECK-NOT: LOAD_32
; CHECK-NOT: STORE_32
; CHECK: RET
entry:
  %p = alloca i32, align 1
  store i32 305419896, ptr %p
  %l = load i32, ptr %p
  ret i32 %l
}
