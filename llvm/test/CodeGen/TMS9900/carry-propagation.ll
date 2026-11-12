; RUN: llc -mtriple=tms9900 -O2 < %s | FileCheck %s
;
; Test 32-bit addition and subtraction carry/borrow propagation using
; TMS9900's hardware Carry flag (JNC / JOC).

target datalayout = "e-m:e-p:16:16-i16:16-a:0:16-n16"

; --- 32-bit add with carry propagation ---
; CHECK-LABEL: add32:
; CHECK-DAG: A{{[ \t]+}}R2,R0
; CHECK-DAG: A{{[ \t]+}}R3,R1
; CHECK: JNC
; CHECK: INC{{[ \t]+}}R0
; CHECK: B{{[ \t]+}}*R11

define i32 @add32(i32 %a, i32 %b) {
  %r = add i32 %a, %b
  ret i32 %r
}

; --- 32-bit sub with borrow propagation ---
; CHECK-LABEL: sub32:
; CHECK-DAG: S{{[ \t]+}}R2,R0
; CHECK-DAG: S{{[ \t]+}}R3,R1
; CHECK: JOC
; CHECK: DEC{{[ \t]+}}R0
; CHECK: B{{[ \t]+}}*R11

define i32 @sub32(i32 %a, i32 %b) {
  %r = sub i32 %a, %b
  ret i32 %r
}

; --- 32-bit add with constant (0x10001 = 65537) ---
; CHECK-LABEL: add32_const:
; CHECK-DAG: INC{{[ \t]+}}R0
; CHECK-DAG: INC{{[ \t]+}}R1
; CHECK: JNC
; CHECK: INC{{[ \t]+}}R0
; CHECK: B{{[ \t]+}}*R11

define i32 @add32_const(i32 %a) {
  %r = add i32 %a, 65537
  ret i32 %r
}

; --- 32-bit sub with constant (0x10001 = 65537 -> add -65537 = 0xFFFEFFFF) ---
; CHECK-LABEL: sub32_const:
; CHECK-DAG: DECT{{[ \t]+}}R0
; CHECK-DAG: DEC{{[ \t]+}}R1
; CHECK: JNC
; CHECK: INC{{[ \t]+}}R0
; CHECK: B{{[ \t]+}}*R11

define i32 @sub32_const(i32 %a) {
  %r = sub i32 %a, 65537
  ret i32 %r
}
