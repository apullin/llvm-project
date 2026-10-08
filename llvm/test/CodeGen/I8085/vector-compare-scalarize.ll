; RUN: llc -mtriple=i8085 -verify-machineinstrs < %s | FileCheck %s
define void @compare(ptr %a, ptr %b, ptr %out) {
  %x = load <2 x i16>, ptr %a, align 1
  %y = load <2 x i16>, ptr %b, align 1
  %c = icmp slt <2 x i16> %x, %y
  %r = sext <2 x i1> %c to <2 x i16>
  store <2 x i16> %r, ptr %out, align 1
  ret void
}
; CHECK-LABEL: compare:
; CHECK: RET
