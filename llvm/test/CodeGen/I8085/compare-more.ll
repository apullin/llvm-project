; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Signed i8 less-than select should use sign checks (XRA/ANI 128) and carry logic.

define i8 @cmp_slt_i8(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_slt_i8:
; CHECK-NOT: SET_
; CHECK: XRA
; CHECK: ANI 128
; CHECK: SUB
; CHECK: JNC
; CHECK-NOT: SET_
; CHECK: RET
  %cmp = icmp slt i8 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

; Signed i8 greater-than select should also use sign checks.

define i8 @cmp_sgt_i8(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_sgt_i8:
; CHECK-NOT: SET_
; CHECK: XRA
; CHECK: ANI 128
; CHECK: SUB
; CHECK: JZ
; CHECK-NOT: SET_
; CHECK: RET
  %cmp = icmp sgt i8 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

; Signed i16 less-than select should check sign mismatch then do 16-bit subtract.

define i8 @cmp_slt_i16(i16 %a, i16 %b) {
; CHECK-LABEL: cmp_slt_i16:
; CHECK-NOT: SET_
; CHECK: XRA
; CHECK: ANI 128
; CHECK: SUB
; CHECK: SBB
; CHECK: JNC
; CHECK-NOT: SET_
; CHECK: RET
  %cmp = icmp slt i16 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

; Unsigned i16 greater-or-equal should use carry from 16-bit subtract.

define i8 @cmp_uge_i16(i16 %a, i16 %b) {
; CHECK-LABEL: cmp_uge_i16:
; CHECK-NOT: SET_
; CHECK: SUB
; CHECK: SBB
; CHECK: JNC
; CHECK-NOT: SET_
; CHECK: RET
  %cmp = icmp uge i16 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}

; Unsigned i8 greater-or-equal should use carry from subtract.

define i8 @cmp_uge_i8(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_uge_i8:
; CHECK-NOT: SET_
; CHECK: SUB
; CHECK: JNC
; CHECK-NOT: SET_
; CHECK: RET
  %cmp = icmp uge i8 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}
