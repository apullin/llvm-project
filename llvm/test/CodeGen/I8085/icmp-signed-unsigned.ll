; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Signed vs unsigned compare paths.

define i8 @cmp_signed_unsigned_i16(i16 %a, i16 %b) {
; CHECK-LABEL: cmp_signed_unsigned_i16:
; CHECK: RET
entry:
  %slt = icmp slt i16 %a, %b
  %ult = icmp ult i16 %a, %b
  %sval = select i1 %slt, i8 1, i8 0
  %uval = select i1 %ult, i8 2, i8 0
  %sum = add i8 %sval, %uval
  ret i8 %sum
}

define i8 @cmp_signed_unsigned_i32(i32 %a, i32 %b) {
; CHECK-LABEL: cmp_signed_unsigned_i32:
; CHECK: RET
entry:
  %sgt = icmp sgt i32 %a, %b
  %ugt = icmp ugt i32 %a, %b
  %sval = select i1 %sgt, i8 3, i8 0
  %uval = select i1 %ugt, i8 4, i8 0
  %sum = add i8 %sval, %uval
  ret i8 %sum
}
