; RUN: llc -mtriple=i8085-unknown-elf -O3 -verify-machineinstrs \
; RUN:   -stop-after=i8085-peephole < %s | FileCheck %s --check-prefix=EARLY
; RUN: llc -mtriple=i8085-unknown-elf -O3 -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefix=LATE

; Keep conditional branches analyzable through generic block placement, then
; fold a branch to a return into the one-byte conditional-return instruction.

define i8 @signed_greater_than(i8 %a, i8 %b) {
; EARLY-LABEL: name: signed_greater_than
; EARLY: JNC
; EARLY-NOT: RNC
; LATE-LABEL: signed_greater_than:
; LATE: RNC
  %cmp = icmp sgt i8 %a, %b
  %result = zext i1 %cmp to i8
  ret i8 %result
}
