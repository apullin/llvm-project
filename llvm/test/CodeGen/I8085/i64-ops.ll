; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; i64 basic ops should lower without libcalls for add/sub/bitwise.

define i64 @i64_arith(i64 %a, i64 %b) {
; CHECK-LABEL: i64_arith:
; CHECK-NOT: CALL
; CHECK: RET
entry:
  %add = add i64 %a, %b
  %sub = sub i64 %add, 42
  %and = and i64 %sub, 305419896
  %or = or i64 %and, %b
  %xor = xor i64 %or, -1
  ret i64 %xor
}

define i8 @i64_cmp_ult(i64 %a, i64 %b) {
; CHECK-LABEL: i64_cmp_ult:
; CHECK-NOT: CALL
; CHECK: RET
entry:
  %cmp = icmp ult i64 %a, %b
  %z = zext i1 %cmp to i8
  ret i8 %z
}
