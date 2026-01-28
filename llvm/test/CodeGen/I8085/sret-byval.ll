; RUN: llc -O0 -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

%S = type { i8, i16, i32 }
%R = type { i16, i16, i32 }

declare void @callee(ptr byval(%S) align 1)

define void @call_byval(ptr %p) {
; CHECK-LABEL: call_byval:
; CHECK: CALL callee
; CHECK: RET
entry:
  call void @callee(ptr byval(%S) align 1 %p)
  ret void
}

define void @fill(ptr sret(%R) %out, i16 %a, i16 %b, i32 %c) {
; CHECK-LABEL: fill:
; CHECK: RET
entry:
  %f0 = getelementptr inbounds %R, ptr %out, i16 0, i32 0
  store i16 %a, ptr %f0, align 1
  %f1 = getelementptr inbounds %R, ptr %out, i16 0, i32 1
  store i16 %b, ptr %f1, align 1
  %f2 = getelementptr inbounds %R, ptr %out, i16 0, i32 2
  store i32 %c, ptr %f2, align 1
  ret void
}

define i32 @call_sret(i16 %a, i16 %b, i32 %c) {
; CHECK-LABEL: call_sret:
; CHECK: CALL fill
; CHECK: RET
entry:
  %tmp = alloca %R, align 1
  call void @fill(ptr sret(%R) %tmp, i16 %a, i16 %b, i32 %c)
  %f2 = getelementptr inbounds %R, ptr %tmp, i16 0, i32 2
  %v = load i32, ptr %f2, align 1
  ret i32 %v
}
