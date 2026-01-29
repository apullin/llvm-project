; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

define i1 @cmp_sext_i8(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_sext_i8:
; CHECK: RET
entry:
  %sa = sext i8 %a to i16
  %sb = sext i8 %b to i16
  %cmp = icmp slt i16 %sa, %sb
  ret i1 %cmp
}

define i1 @cmp_zext_i8(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_zext_i8:
; CHECK: RET
entry:
  %za = zext i8 %a to i16
  %zb = zext i8 %b to i16
  %cmp = icmp ult i16 %za, %zb
  ret i1 %cmp
}

define i1 @cmp_mixed_i16(i16 %a, i16 %b) {
; CHECK-LABEL: cmp_mixed_i16:
; CHECK: RET
entry:
  %tr = trunc i16 %a to i8
  %se = sext i8 %tr to i16
  %cmp = icmp sgt i16 %se, %b
  ret i1 %cmp
}
