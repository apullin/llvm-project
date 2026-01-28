; RUN: llc -mattr=i8085,sram -O0 < %s -march=i8085 -verify-machineinstrs | FileCheck %s --check-prefix=O0
; RUN: llc -mattr=i8085,sram -O2 < %s -march=i8085 -verify-machineinstrs | FileCheck %s --check-prefix=O2

; Ensure the pipeline is stable across optimization levels.

define i16 @opt_mix(i16 %a, i16 %b, i8 %c) {
; O0-LABEL: opt_mix:
; O0: RET
; O2-LABEL: opt_mix:
; O2: RET
entry:
  %cond = icmp ult i8 %c, 10
  %sel = select i1 %cond, i16 %a, i16 %b
  br label %loop

loop:
  %i = phi i16 [ 0, %entry ], [ %i.next, %loop ]
  %acc = phi i16 [ 0, %entry ], [ %acc.next, %loop ]
  %acc.next = add i16 %acc, %sel
  %i.next = add i16 %i, 1
  %cmp = icmp ult i16 %i.next, 4
  br i1 %cmp, label %loop, label %exit

exit:
  ret i16 %acc.next
}
