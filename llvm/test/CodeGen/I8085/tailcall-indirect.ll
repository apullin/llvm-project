; RUN: llc -mtriple=i8085 -verify-machineinstrs < %s | FileCheck %s

define void @ordinary(ptr %p) {
  tail call void %p()
  ret void
}
; CHECK-LABEL: ordinary:
; Indirect calls synthesize a return address before PCHL, not a direct CALL.
; CHECK: PUSH B
; CHECK: PCHL
; CHECK: RET
