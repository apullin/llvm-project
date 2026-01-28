; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Indirect calls should lower without crashes.

define i16 @call_indirect(ptr %fn, i16 %a) {
; CHECK-LABEL: call_indirect:
; CHECK: RET
entry:
  %r = call i16 %fn(i16 %a)
  ret i16 %r
}
