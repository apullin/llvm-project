; RUN: llc -mtriple=i8085 -O2 < %s | FileCheck %s

define i16 @caller() {
entry:
  %call = tail call i16 @callee()
  ret i16 %call
}

define i16 @callee() {
entry:
  ret i16 7
}

; CHECK-LABEL: caller:
; CHECK-NOT: CALL
; CHECK: JMP callee
; CHECK-LABEL: callee:
