; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Select lowering for i16 should emit control flow and return.

define i16 @select_i16(i16 %a, i16 %b, i1 %c) {
; CHECK-LABEL: select_i16:
; CHECK: JNZ
; CHECK: RET
entry:
  %sel = select i1 %c, i16 %a, i16 %b
  ret i16 %sel
}
