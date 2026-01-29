; RUN: llc -O2 -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Stress ABI lowering for byval, aggregate returns, and varargs at -O2.

%S = type { i8, i16, i8 }
%L = type { i32, i16, i8 }

declare void @callee_byval(ptr byval(%S) align 1, i16, i32, i8)

define void @caller_byval(ptr %p, i16 %a, i32 %b, i8 %c) {
; CHECK-LABEL: caller_byval:
; CHECK: CALL callee_byval
; CHECK: RET
entry:
  call void @callee_byval(ptr byval(%S) align 1 %p, i16 %a, i32 %b, i8 %c)
  ret void
}

define %L @ret_large(i32 %a, i16 %b, i8 %c) {
; CHECK-LABEL: ret_large:
; CHECK: RET
entry:
  %s0 = insertvalue %L undef, i32 %a, 0
  %s1 = insertvalue %L %s0, i16 %b, 1
  %s2 = insertvalue %L %s1, i8 %c, 2
  ret %L %s2
}

define i16 @use_ret_large(i32 %a, i16 %b, i8 %c) {
; CHECK-LABEL: use_ret_large:
; CHECK: CALL ret_large
; CHECK: RET
entry:
  %r = call %L @ret_large(i32 %a, i16 %b, i8 %c)
  %v = extractvalue %L %r, 1
  ret i16 %v
}

declare void @vfoo(i16, ...)

define void @caller_varargs_mix(i16 %a, i8 %b, i32 %c, i64 %d) {
; CHECK-LABEL: caller_varargs_mix:
; CHECK: CALL vfoo
; CHECK: RET
entry:
  call void (i16, ...) @vfoo(i16 %a, i8 %b, i32 %c, i64 %d, i16 4660, i8 7)
  ret void
}
