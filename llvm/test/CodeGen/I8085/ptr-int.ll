; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Pointer/integer casts should lower without libcalls.

define i16 @ptr_to_int(ptr %p) {
; CHECK-LABEL: ptr_to_int:
; CHECK: RET
entry:
  %i = ptrtoint ptr %p to i16
  ret i16 %i
}

define ptr @int_to_ptr(i16 %x) {
; CHECK-LABEL: int_to_ptr:
; CHECK: RET
entry:
  %p = inttoptr i16 %x to ptr
  ret ptr %p
}
