; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

%S = type { i8, i16, i32 }
%R = type { i16, i16, i32 }

; Indirect void call.

define void @call_indirect_void(ptr %fn, i16 %a) {
; CHECK-LABEL: call_indirect_void:
; CHECK: PCHL
; CHECK: RET
entry:
  call void %fn(i16 %a)
  ret void
}

; Indirect byval call.

define void @call_indirect_byval(ptr %fn, ptr %p) {
; CHECK-LABEL: call_indirect_byval:
; CHECK: PCHL
; CHECK: RET
entry:
  call void %fn(ptr byval(%S) align 1 %p)
  ret void
}

; Indirect sret call.

define i32 @call_indirect_sret(ptr %fn, i16 %a, i16 %b, i32 %c) {
; CHECK-LABEL: call_indirect_sret:
; CHECK: PCHL
; CHECK: RET
entry:
  %tmp = alloca %R, align 1
  call void %fn(ptr sret(%R) %tmp, i16 %a, i16 %b, i32 %c)
  %f2 = getelementptr inbounds %R, ptr %tmp, i16 0, i32 2
  %v = load i32, ptr %f2, align 1
  ret i32 %v
}
