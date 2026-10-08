; RUN: llc -mtriple=i8085 -verify-machineinstrs < %s | FileCheck %s
define i8 @large_frame() {
  %a = alloca [40000 x i8], align 1
  %p = getelementptr [40000 x i8], ptr %a, i16 0, i16 39999
  store volatile i8 19, ptr %p, align 1
  %r = load volatile i8, ptr %p, align 1
  ret i8 %r
}
; CHECK-LABEL: large_frame:
; CHECK: RET
