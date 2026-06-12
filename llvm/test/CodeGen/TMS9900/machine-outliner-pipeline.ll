; RUN: llc -mtriple=tms9900 -enable-machine-outliner=always -verify-machineinstrs %s -o - | FileCheck %s

target triple = "tms9900"

define i16 @tail1(i16 %x) minsize optsize {
entry:
  %a = add i16 %x, 5
  %b = xor i16 %a, 4660
  %c = add i16 %b, 7
  %d = xor i16 %c, 22136
  ret i16 %d
}

define i16 @tail2(i16 %x) minsize optsize {
entry:
  %a = add i16 %x, 5
  %b = xor i16 %a, 4660
  %c = add i16 %b, 7
  %d = xor i16 %c, 22136
  ret i16 %d
}

; CHECK-LABEL: tail1:
; CHECK: B @[[OUTLINED:OUTLINED_FUNCTION_[0-9]+]]

; CHECK-LABEL: tail2:
; CHECK: B @[[OUTLINED]]

; CHECK: [[OUTLINED]]:
; CHECK: AI R0,5
; CHECK: LI R1,4660
; CHECK: XOR R1,R0
; CHECK: AI R0,7
; CHECK: XOR R1,R0
; CHECK: B *R11
