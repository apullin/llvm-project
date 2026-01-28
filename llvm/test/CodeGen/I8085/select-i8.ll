; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Select lowering for i8 should emit control flow and return.

define i8 @select_i8(i8 %a, i8 %b, i1 %c) {
; CHECK-LABEL: select_i8:
; CHECK: JNZ
; CHECK: RET
entry:
  %sel = select i1 %c, i8 %a, i8 %b
  ret i8 %sel
}
