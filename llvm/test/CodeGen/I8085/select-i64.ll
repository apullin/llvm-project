; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Select lowering for i64 should not leave compare/branch pseudos.

define i64 @select_i64(i64 %a, i64 %b, i1 %c) {
; CHECK-LABEL: select_i64:
; CHECK-NOT: SET_
; CHECK-NOT: JMP_32_IF
; CHECK: RET
entry:
  %sel = select i1 %c, i64 %a, i64 %b
  ret i64 %sel
}
