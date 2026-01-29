; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

%S1 = type { i8, i8 }
%S2 = type { i16, i8 }

define %S1 @ret_s1(i8 %a, i8 %b) {
; CHECK-LABEL: ret_s1:
; CHECK: RET
entry:
  %s = insertvalue %S1 undef, i8 %a, 0
  %t = insertvalue %S1 %s, i8 %b, 1
  ret %S1 %t
}

define %S2 @ret_s2(i16 %a, i8 %b) {
; CHECK-LABEL: ret_s2:
; CHECK: RET
entry:
  %s = insertvalue %S2 undef, i16 %a, 0
  %t = insertvalue %S2 %s, i8 %b, 1
  ret %S2 %t
}

define i8 @use_s1(i8 %a, i8 %b) {
; CHECK-LABEL: use_s1:
; CHECK: CALL ret_s1
; CHECK: RET
entry:
  %r = call %S1 @ret_s1(i8 %a, i8 %b)
  %v = extractvalue %S1 %r, 0
  ret i8 %v
}

define i16 @use_s2(i16 %a, i8 %b) {
; CHECK-LABEL: use_s2:
; CHECK: CALL ret_s2
; CHECK: RET
entry:
  %r = call %S2 @ret_s2(i16 %a, i8 %b)
  %v = extractvalue %S2 %r, 0
  ret i16 %v
}
