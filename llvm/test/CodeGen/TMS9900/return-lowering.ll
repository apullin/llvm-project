; RUN: llc -march=tms9900 -O0 -verify-machineinstrs < %s | FileCheck %s

; Four 16-bit return units fit exactly in R0-R3.
; CHECK-LABEL: return_four_words:
; CHECK: B{{[ \t]+}}*R11
define { i16, i16, i16, i16 } @return_four_words(i16 %a, i16 %b,
                                                   i16 %c, i16 %d) {
  %r0 = insertvalue { i16, i16, i16, i16 } poison, i16 %a, 0
  %r1 = insertvalue { i16, i16, i16, i16 } %r0, i16 %b, 1
  %r2 = insertvalue { i16, i16, i16, i16 } %r1, i16 %c, 2
  %r3 = insertvalue { i16, i16, i16, i16 } %r2, i16 %d, 3
  ret { i16, i16, i16, i16 } %r3
}

; A fifth return unit cannot be assigned to a return register. SelectionDAG
; must demote the return to a hidden pointer instead of reaching LowerReturn
; with an unassignable value.
; CHECK-LABEL: return_five_words:
; CHECK: MOV{{[ \t]+}}R0,[[SRET:R[0-9]+]]
; CHECK-DAG: MOV{{[ \t]+}}{{R[0-9]+}},*[[SRET]]
; CHECK-DAG: MOV{{[ \t]+}}{{R[0-9]+}},@2([[SRET]])
; CHECK-DAG: MOV{{[ \t]+}}{{R[0-9]+}},@4([[SRET]])
; CHECK-DAG: MOV{{[ \t]+}}{{R[0-9]+}},@6([[SRET]])
; CHECK-DAG: MOV{{[ \t]+}}{{R[0-9]+}},@8([[SRET]])
; CHECK: B{{[ \t]+}}*R11
define { i16, i16, i16, i16, i16 } @return_five_words(
    i16 %a, i16 %b, i16 %c, i16 %d, i16 %e) {
  %r0 = insertvalue { i16, i16, i16, i16, i16 } poison, i16 %a, 0
  %r1 = insertvalue { i16, i16, i16, i16, i16 } %r0, i16 %b, 1
  %r2 = insertvalue { i16, i16, i16, i16, i16 } %r1, i16 %c, 2
  %r3 = insertvalue { i16, i16, i16, i16, i16 } %r2, i16 %d, 3
  %r4 = insertvalue { i16, i16, i16, i16, i16 } %r3, i16 %e, 4
  ret { i16, i16, i16, i16, i16 } %r4
}
