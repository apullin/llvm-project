; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Indirect branches should lower to PCHL.

define i16 @indirectbr_test(i1 %c) {
; CHECK-LABEL: indirectbr_test:
; CHECK: PCHL
; CHECK: RET
entry:
  br i1 %c, label %t, label %f

t:
  br label %dispatch

f:
  br label %dispatch

dispatch:
  %p = phi ptr [ blockaddress(@indirectbr_test, %case1), %t ],
                [ blockaddress(@indirectbr_test, %case2), %f ]
  indirectbr ptr %p, [label %case1, label %case2]

case1:
  ret i16 1

case2:
  ret i16 2
}
